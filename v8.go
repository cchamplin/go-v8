// Package v8 provides a Go API for interacting with the V8 javascript library.
package v8

// #include <stdlib.h>
// #include "v8wrap.h"
// #cgo CXXFLAGS: -std=c++11
// #cgo LDFLAGS: -lv8_base -lv8_libbase -lv8_snapshot -lv8_libplatform -ldl -pthread
import "C"

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"path"
	"reflect"
	"runtime"
	"sync"
	"text/template"
	"unsafe"
)

var contexts = make(map[uint]*V8Context)
var contextsMutex sync.RWMutex
var highestContextId uint

// NO_FILE is a constant indicating that a particular script evaluation is not associated
// with any file.
const NO_FILE = ""

// Value represents a handle to a V8::Value.  It is associated with a particular
// context and attempts to use it with a different context will fail.
type Value struct {
	ptr C.PersistentValuePtr
	ctx *V8Context
}

// Get returns the given field of the object.
// TODO(mag): optimize.
func (v *Value) Get(field string) (*Value, error) {
	if v == nil {
		panic("nil value")
	}
	if v.ctx == nil || v.ptr == nil {
		panic("Value or context were reset.")
	}

	fields, err := v.Burst()
	if err != nil {
		return nil, err
	}
	res, exists := fields[field]
	if !exists || res == nil {
		return nil, fmt.Errorf("field '%s' is undefined", field)
	}
	return res, nil
}

// Set will set the given field of the provided object.
func (v *Value) Set(field string, val *Value) error {
	if v.ctx == nil || v.ptr == nil {
		panic("Value or context were reset")
	}
	fieldPtr := C.CString(field)
	defer C.free(unsafe.Pointer(fieldPtr))
	errmsg := C.v8_setPersistentField(v.ctx.v8context, v.ptr, fieldPtr, val.ptr)
	if errmsg != nil {
		return errors.New(C.GoString(errmsg))
	}
	return nil
}

// Function is the callback signature for functions that are registered with
// a V8 context.  Arguments and return values are POD serialized via JSON and
// unmarshaled into Go interface{}s via json.Unmarshal.
type Function func(...interface{}) interface{}

// Loc defines a script location.
type Loc struct {
	Funcname, Filename string
	Line, Column       int
}

// RawFunction is the callback signature for functions that are registered with
// a V8 context via AddRawFunc().  The first argument is the script location
// that is calling the regsitered RawFunction, and remaining arguments and
// return values are Value objects that represent handles to data within the V8
// context.  If the function is called directly from Go (e.g. via Apply()), then
// "from" will be empty. Never return a Value from a different V8 context.
type RawFunction func(from Loc, args ...*Value) (*Value, error)

// V8Isolate refers to a specific v8 Isolate for the execution of the v8 engine.
// All contexts will run within a single Isolate
type V8Isolate struct {
	v8isolate C.IsolatePtr
}

// V8Context is a handle to a v8 context.
type V8Context struct {
	id        uint
	v8context C.ContextPtr
	v8isolate *V8Isolate
	funcs     map[string]Function
	rawFuncs  map[string]RawFunction
	values    map[*Value]bool
	valuesMu  *sync.Mutex
}

var platform C.PlatformPtr
var defaultIsolate *V8Isolate

func init() {
	platform = C.v8_init()
	defaultIsolate = NewIsolate()
}

// NewIsolate returns a new V8Isolate
func NewIsolate() *V8Isolate {
	res := &V8Isolate{C.v8_create_isolate()}
	runtime.SetFinalizer(res, func(i *V8Isolate) {
		C.v8_release_isolate(i.v8isolate)
	})
	return res
}

// NewContext creates a V8 context in a default isolate
// and returns a handle to it.
func NewContext() *V8Context {
	return NewContextInIsolate(defaultIsolate)
}

// NewContextInIsolate creates a V8 context in a given isolate
// and returns a handle to it.
func NewContextInIsolate(isolate *V8Isolate) *V8Context {
	v := &V8Context{
		v8context: C.v8_create_context(isolate.v8isolate),
		v8isolate: isolate,
		funcs:     make(map[string]Function),
		rawFuncs:  make(map[string]RawFunction),
		values:    make(map[*Value]bool),
		valuesMu:  &sync.Mutex{},
	}

	contextsMutex.Lock()
	highestContextId++
	v.id = highestContextId
	contexts[v.id] = v
	contextsMutex.Unlock()

	runtime.SetFinalizer(v, func(p *V8Context) {
		p.Destroy()
	})
	return v
}

// Destroy releases the context handle and all the values allocated within the context.
// NOTE: The context can't be used for anything after this function is called.
func (v *V8Context) Destroy() error {
	if v.v8context == nil {
		return errors.New("Context is uninitialized.")
	}
	v.ClearValues()

	contextsMutex.Lock()
	delete(contexts, v.id)
	contextsMutex.Unlock()

	C.v8_release_context(v.v8context)
	v.v8context = nil
	v.v8isolate = nil
	return nil
}

// ClearValues releases all the values allocated in this context.
func (v *V8Context) ClearValues() error {
	if v.v8context == nil {
		panic("Context is uninitialized.")
	}
	v.valuesMu.Lock()
	for val, _ := range v.values {
		v.releaseValueLocked(val)
	}
	v.valuesMu.Unlock()
	return nil
}

// ReleaseValue releases the v8 handle that val points to.
// NOTE: The val object can't be used after this function is called on it.
func (v *V8Context) ReleaseValue(val *Value) error {
	if v.v8context == nil {
		panic("Context is uninitialized.")
	}
	v.valuesMu.Lock()
	defer v.valuesMu.Unlock()
	if val.ptr == nil {
		return errors.New("Value has been already released.")
	}
	v.releaseValueLocked(val)
	return nil
}

// Dispose of the persistent object and free the allocated handle.
func (v *V8Context) releaseValueLocked(val *Value) {
	val.ctx = nil
	delete(v.values, val)
	C.v8_release_persistent(v.v8context, val.ptr)
}

func (v *V8Context) newValue(ptr C.PersistentValuePtr) *Value {
	res := &Value{ptr, v}
	v.valuesMu.Lock()
	v.values[res] = true
	v.valuesMu.Unlock()
	return res
}

// Terminate Stops the computation running inside the isolate.
func (iso *V8Isolate) Terminate() {
	C.v8_terminate(iso.v8isolate)
}

// Eval executes the provided javascript within the V8 context.  The javascript
// is executed as if it was from the specified file, so that any errors or stack
// traces are annotated with the corresponding file/line number.
//
// The result of the javascript is returned as POD serialized via JSON and
// unmarshaled back into Go, otherwise an error is returned.
func (v *V8Context) Eval(javascript string, filename string) (res interface{}, err error) {
	if v.v8context == nil {
		panic("Context is uninitialized.")
	}
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()
	jsPtr := C.CString(javascript)
	defer C.free(unsafe.Pointer(jsPtr))
	var filenamePtr *C.char
	if len(filename) > 0 {
		filenamePtr = C.CString(filename)
		defer C.free(unsafe.Pointer(filenamePtr))
	}
	ret := C.v8_execute(v.v8context, jsPtr, filenamePtr)
	if ret != nil {
		out := C.GoString(ret)
		if out != "" {
			C.free(unsafe.Pointer(ret))
			err := json.Unmarshal([]byte(out), &res)
			return res, err
		}
		return out, nil
	}
	ret = C.v8_error(v.v8context)
	out := C.GoString(ret)
	C.free(unsafe.Pointer(ret))
	return nil, errors.New(out)
}

func (v *V8Context) convertToValue(e error) *Value {
	strdata, err := json.Marshal(e.Error())
	if err != nil {
		panic(err)
	}

	val, err := v.FromJSON(string(strdata))
	if err != nil {
		panic(err)
	}
	return val
}

func (v *V8Context) throw(err error) {
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()
	msg := C.CString(err.Error())
	defer C.free(unsafe.Pointer(msg))
	C.v8_throw(v.v8context, msg)
}

// Run will call the named function within the v8 context with the
// specified parameters.
// Parameters are serialized via JSON.
func (v *V8Context) Run(funcname string, args ...interface{}) (interface{}, error) {
	if v.v8context == nil {
		panic("Context is uninitialized.")
	}

	var cmd bytes.Buffer

	fmt.Fprint(&cmd, funcname, "(")
	for i, arg := range args {
		if i > 0 {
			fmt.Fprint(&cmd, ",")
		}
		err := json.NewEncoder(&cmd).Encode(arg)
		if err != nil {
			return nil, err
		}
	}
	fmt.Fprint(&cmd, ")")
	return v.Eval(cmd.String(), fmt.Sprintf("[RUN:%v]", funcname))
}

// CreateJS evalutes the specified javascript object and returns a handle to the
// result.  This allows:
//   (1) Creating objects using JS notation rather than JSON notation:
//        val = ctx.CreateJS("{a:1}", NO_FILE)
//   (2) Creating function values:
//        val = ctx.CreateJS("function(a,b) { return a+b; }", NO_FILE)
//   (3) Creating objects that reference existing state in the context:
//        val = ctx.CreateJS("{a:some_func, b:console.log}", NO_FILE)
// The filename parameter may be specified to provide additional debugging in
// the case of failures.
func (v *V8Context) CreateJS(js, filename string) (*Value, error) {
	if v.v8context == nil {
		panic("Context is uninitialized.")
	}
	script := fmt.Sprintf(`(function() { return %s; })()`, js)
	return v.EvalRaw(script, filename)
}

// EvalRaw executes the provided javascript within the V8 context.  The
// javascript is executed as if it was from the specified file, so that any
// errors or stack traces are annotated with the corresponding file/line number.
//
// The result of the javascript is returned as a handle to the result in the V8
// engine if it succeeded, otherwise an error is returned.  Unlike Eval, this
// does not do any JSON marshalling/unmarshalling of the results
func (v *V8Context) EvalRaw(js string, filename string) (*Value, error) {
	if v.v8context == nil {
		panic("Context is uninitialized.")
	}
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

	jsPtr := C.CString(js)
	defer C.free(unsafe.Pointer(jsPtr))

	filenamePtr := C.CString(filename)
	defer C.free(unsafe.Pointer(filenamePtr))

	ret := C.v8_eval(v.v8context, jsPtr, filenamePtr)
	if ret == nil {
		err := C.v8_error(v.v8context)
		defer C.free(unsafe.Pointer(err))
		return nil, fmt.Errorf("Failed to execute JS (%s): %s", filename, C.GoString(err))
	}

	val := v.newValue(ret)

	return val, nil
}

// Apply will execute a JS Function with the specified 'this' context and
// parameters. If 'this' is nil, then the function is executed in the global
// scope.  f must be a Value handle that holds a JS function.  Other
// parameters may be any Value.
func (v *V8Context) Apply(f, this *Value, args ...*Value) (*Value, error) {
	if v.v8context == nil {
		panic("Context is uninitialized.")
	}
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()

	// always allocate at least one so &argPtrs[0] works.
	argPtrs := make([]C.PersistentValuePtr, len(args)+1)
	for i := range args {
		argPtrs[i] = args[i].ptr
	}
	var thisPtr C.PersistentValuePtr
	if this != nil {
		thisPtr = this.ptr
	}
	ret := C.v8_apply(v.v8context, f.ptr, thisPtr, C.int(len(args)), &argPtrs[0])
	if ret == nil {
		err := C.v8_error(v.v8context)
		defer C.free(unsafe.Pointer(err))
		return nil, errors.New(C.GoString(err))
	}

	val := v.newValue(ret)

	return val, nil
}

// Terminate forcibly stops execution of a V8 context.  This can be safely run
// from any goroutine or thread.  If the V8 context is not running anything,
// this will have no effect.
func (v *V8Context) Terminate() {
	v.v8isolate.Terminate()
}

// AddFunc adds a function into the V8 context.
func (v *V8Context) AddFunc(name string, f Function) error {
	v.funcs[name] = f
	jsCall := fmt.Sprintf(`function %v() {
		  return _go_call(%v, "%v", JSON.stringify([].slice.call(arguments)));
		}`, name, v.id, name)
	funcname, filepath, line := funcInfo(f)
	_, err := v.Eval(jsCall, fmt.Sprintf("native callback to %s [%s:%d]",
		path.Ext(funcname)[1:], path.Base(filepath), line))
	return err
}

// AddRawFunc adds a raw function into the V8 context.
func (v *V8Context) AddRawFunc(name string, f RawFunction) error {
	v.rawFuncs[name] = f
	jsCall := fmt.Sprintf(`function %v() {
			return _go_call_raw(%v, "%v", [].slice.call(arguments));
		}`, name, v.id, name)
	funcname, filepath, line := funcInfo(f)
	_, err := v.Eval(jsCall, fmt.Sprintf("native callback to %s [%s:%d]",
		path.Ext(funcname)[1:], path.Base(filepath), line))
	return err
}

// CreateRawFunc adds a raw function into the V8 context without polluting the
// namespace.  The only reference to the function is returned as a *v8.Value.
func (v *V8Context) CreateRawFunc(f RawFunction) (fn *Value, err error) {
	funcname, filepath, line := funcInfo(f)
	name := fmt.Sprintf("RawFunc:%s@%s:%d", funcname, path.Base(filepath), line)
	name = template.JSEscapeString(name)
	v.rawFuncs[name] = f
	jscode := fmt.Sprintf(`(function() {
		return _go_call_raw(%v, "%v", [].slice.call(arguments));
	})`, v.id, name)
	return v.EvalRaw(jscode, name)
}

// Given any function, it will return the name (a/b/pkg.name), full filename
// path, and line number of that function.
func funcInfo(function interface{}) (name, filepath string, line int) {
	ptr := reflect.ValueOf(function)
	f := runtime.FuncForPC(ptr.Pointer())
	file, line := f.FileLine(f.Entry())
	return f.Name(), file, line
}

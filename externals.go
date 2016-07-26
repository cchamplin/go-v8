package v8

// #include <stdlib.h>
// #include "v8wrap.h"
import "C"

import (
	"encoding/json"
	"fmt"
	"reflect"
	"runtime"
	"unsafe"
)

// NOTE: We release the v8 Value here, and remove the internal from the list
//export _go_v8_dispose_wrapped
func _go_v8_dispose_wrapped(identifier C.uint) {

	runtime.UnlockOSThread()
	defer runtime.LockOSThread()

	internalsMu.Lock()
	defer internalsMu.Unlock()
	wrapped := internals[uint(identifier)]

	if wrapped.disposeCallback != nil {
		wrapped.disposeCallback(wrapped.internal, wrapped.external)
	}

	wrapped.ctx.ReleaseValue(wrapped.external)

	delete(internals, uint(identifier))
}

//export _go_v8_property_get
func _go_v8_property_get(identifier C.ObjectPtr, name *C.char) C.PersistentValuePtr {
	runtime.UnlockOSThread()
	defer runtime.LockOSThread()

	internalsMu.RLock()

	wrapped, ok := internals[uint(identifier)] //internals[uintptr(identifier)]
	if !ok {
		panic(fmt.Errorf("Reference to unknown object: %d.", uint(identifier)))
	}
	internalsMu.RUnlock()
	propertyName := C.GoString(name)
	if wrapped.getter != nil {

		res, err := wrapped.getter(wrapped.ctx, wrapped.internal, propertyName)

		if err != nil {
			wrapped.ctx.throw(err)
			return nil
		}

		if res == nil {
			return nil
		}

		if res.ctx.v8context != wrapped.ctx.v8context {
			panic(fmt.Errorf("Error processing return value of raw function callback %s: "+
				"Return value was generated from another context.", propertyName))
		}
		return res.ptr
	}
	return nil
}

//export _go_v8_callback
func _go_v8_callback(ctxID uint, name, args *C.char) *C.char {
	runtime.UnlockOSThread()
	defer runtime.LockOSThread()

	contextsMutex.RLock()
	c := contexts[ctxID]
	contextsMutex.RUnlock()
	f := c.funcs[C.GoString(name)]
	if f != nil {
		var argv []interface{}
		json.Unmarshal([]byte(C.GoString(args)), &argv)
		ret := f(argv...)
		if ret != nil {
			b, _ := json.Marshal(ret)
			return C.CString(string(b))
		}
		return nil
	}
	return C.CString("undefined")
}

// TODO(mag): catch all panics in go functions, that are called from C code.
//export _go_v8_callback_raw
func _go_v8_callback_raw(
	ctxID uint,
	name *C.char,
	callerFuncName, callerScriptName *C.char,
	callerLineNumber, callerColumn C.int,
	argc C.int,
	argvptr C.PersistentValuePtr,
) C.PersistentValuePtr {
	runtime.UnlockOSThread()
	defer runtime.LockOSThread()

	funcname := C.GoString(name)
	//	fmt.Printf("Callback for %s  %s\n", ctxId, funcname)

	caller := Loc{
		Funcname: C.GoString(callerFuncName),
		Filename: C.GoString(callerScriptName),
		Line:     int(callerLineNumber),
		Column:   int(callerColumn),
	}

	contextsMutex.RLock()
	ctx := contexts[ctxID]
	contextsMutex.RUnlock()
	//for id, _ := range ctx.rawFuncs {
	//	fmt.Printf("Have function: %s\n", id)
	//}
	function := ctx.rawFuncs[funcname]

	var argv []C.PersistentValuePtr
	sliceHeader := (*reflect.SliceHeader)((unsafe.Pointer(&argv)))
	sliceHeader.Cap = int(argc)
	sliceHeader.Len = int(argc)
	sliceHeader.Data = uintptr(unsafe.Pointer(argvptr))

	if function == nil {
		panic(fmt.Errorf("No such registered raw function: %s", C.GoString(name)))
	}

	args := make([]*Value, argc)
	for i := 0; i < int(argc); i++ {
		args[i] = ctx.newValue(argv[i])
	}

	res, err := function(caller, args...)

	if err != nil {
		ctx.throw(err)
		return nil
	}

	if res == nil {
		return nil
	}

	if ctx.v8context == nil {
		panic(fmt.Errorf("Error processing return value of raw function callback %s: "+
			"Context in invalid state.", C.GoString(name)))
	}
	if res.ctx == nil {
		panic(fmt.Errorf("Error processing return value of raw function callback %s: "+
			"Return value contains no context information, was it released?.", C.GoString(name)))
	}
	if res.ctx.v8context != ctx.v8context {
		panic(fmt.Errorf("Error processing return value of raw function callback %s: "+
			"Return value was generated from another context.", C.GoString(name)))
	}
	if res.weak {
		res.ctx = nil
	}
	return res.ptr
}

//export _go_v8_callback_wrapped
func _go_v8_callback_wrapped(
	identifier C.ulonglong,
	//name *C.char
	fn C.ulonglong,
	callerFuncName, callerScriptName *C.char,
	callerLineNumber, callerColumn C.int,
	argc C.int,
	argvptr C.PersistentValuePtr,
) C.PersistentValuePtr {
	runtime.UnlockOSThread()
	defer runtime.LockOSThread()

	caller := Loc{
		Funcname: C.GoString(callerFuncName),
		Filename: C.GoString(callerScriptName),
		Line:     int(callerLineNumber),
		Column:   int(callerColumn),
	}

	//  TODO this is two map lookups for a single callback fubd a way to improve it
	internalsMu.RLock()
	wrapped := internals[uint(identifier)] // *(*V8Wrapped)(unsafe.Pointer(identifier))
	internalsMu.RUnlock()
	ctx := wrapped.ctx
	//fmt.Printf("cb: %d %d", uint(identifier), uint(fn))
	function := wrapped.prototype.funcs[uint(fn)] //*(*WrappedRawFunction)(unsafe.Pointer(fn))

	var argv []C.PersistentValuePtr
	sliceHeader := (*reflect.SliceHeader)((unsafe.Pointer(&argv)))
	sliceHeader.Cap = int(argc)
	sliceHeader.Len = int(argc)
	sliceHeader.Data = uintptr(unsafe.Pointer(argvptr))

	if function == nil {
		panic(fmt.Errorf("Could not locate assigned callback function"))
	}

	args := make([]*Value, argc)
	for i := 0; i < int(argc); i++ {
		args[i] = ctx.newValue(argv[i])
	}

	res, err := function(wrapped.internal, caller, args...)

	if err != nil {
		ctx.throw(err)
		return nil
	}

	if res == nil {
		return nil
	}

	if ctx.v8context == nil {
		panic(fmt.Errorf("Error processing return value of wrapped function callback" +
			"Context in invalid state."))
	}
	if res.ctx == nil {
		panic(fmt.Errorf("Error processing return value of wrapped function callback" +
			"Return value contains no context information, was it released?."))
	}
	if res.ctx.v8context != ctx.v8context {
		panic(fmt.Errorf("Error processing return value of wrapped function callback" +
			"Return value was generated from another context."))
	}
	if res.weak {
		res.ctx = nil
	}
	return res.ptr
}

//export _go_v8_construct_wrapped
func _go_v8_construct_wrapped(
	ctxID uint,
	identifier *C.char,
	callerFuncName, callerScriptName *C.char,
	callerLineNumber, callerColumn C.int,
	argc C.int,
	argvptr C.PersistentValuePtr,
) C.PersistentValuePtr {
	runtime.UnlockOSThread()
	defer runtime.LockOSThread()

	caller := Loc{
		Funcname: C.GoString(callerFuncName),
		Filename: C.GoString(callerScriptName),
		Line:     int(callerLineNumber),
		Column:   int(callerColumn),
	}

	contextsMutex.RLock()
	ctx := contexts[ctxID]
	contextsMutex.RUnlock()
	className := C.GoString(identifier)

	ctx.prototypesMu.RLock()
	proto := ctx.prototypes[className]
	ctx.prototypesMu.RUnlock()

	function := proto.constructor

	var argv []C.PersistentValuePtr
	sliceHeader := (*reflect.SliceHeader)((unsafe.Pointer(&argv)))
	sliceHeader.Cap = int(argc)
	sliceHeader.Len = int(argc)
	sliceHeader.Data = uintptr(unsafe.Pointer(argvptr))

	if function == nil {
		panic(fmt.Errorf("No such registered raw function: %s", className))
	}

	args := make([]*Value, argc)
	for i := 0; i < int(argc); i++ {
		args[i] = ctx.newValue(argv[i])
	}
	var res *Value
	var err error
	if argc > 1 {
		res, err = function(caller, proto, args[0], args[1:]...)
	} else {
		res, err = function(caller, proto, args[0], nil)
	}

	if err != nil {
		ctx.throw(err)
		return nil
	}

	if res == nil {
		return nil
	}

	if ctx.v8context == nil {
		panic(fmt.Errorf("Error processing return value of raw function callback %s: "+
			"Context in invalid state.", className))
	}
	if res.ctx == nil {
		panic(fmt.Errorf("Error processing return value of raw function callback %s: "+
			"Return value contains no context information, was it released?.", className))
	}
	if res.ctx.v8context != ctx.v8context {
		panic(fmt.Errorf("Error processing return value of raw function callback %s: "+
			"Return value was generated from another context.", className))
	}
	if res.weak {
		res.ctx = nil
	}
	return res.ptr
}

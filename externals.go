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

	caller := Loc{
		Funcname: C.GoString(callerFuncName),
		Filename: C.GoString(callerScriptName),
		Line:     int(callerLineNumber),
		Column:   int(callerColumn),
	}

	contextsMutex.RLock()
	ctx := contexts[ctxID]
	contextsMutex.RUnlock()

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


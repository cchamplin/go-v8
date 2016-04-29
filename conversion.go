package v8

// #include <stdlib.h>
// #include "v8wrap.h"
import "C"

import (
	"encoding/json"
	"errors"
	"fmt"
	"reflect"
	"text/template"
	"unsafe"
)

// ToValue attempts to convert a native Go value into a *Value.  If the native
// value is a RawFunction, it will create a function Value.  Otherwise, the
// value must be JSON serializable and the corresponding JS object will be
// constructed.
// NOTE: If the original object has several references to the same object,
// in the resulting JS object those references will become independent objects.
func (v *V8Context) ToValue(val interface{}) (*Value, error) {
	if fn, isFunction := val.(func(Loc, ...*Value) (*Value, error)); isFunction {
		return v.CreateRawFunc(fn)
	}
	data, err := json.Marshal(val)
	if err != nil {
		return nil, fmt.Errorf("Cannot marshal value as JSON: %v\nVal: %#v", err, val)
	}
	return v.FromJSON(string(data))
}

// FromJSON parses a JSON string and returns a Value that references the parsed
// data in the V8 context.
func (v *V8Context) FromJSON(s string) (*Value, error) {
	if v.v8context == nil {
		panic("Context is uninitialized.")
	}
	return v.EvalRaw("JSON.parse('"+template.JSEscapeString(s)+"')", "FromJSON")
}

// ToJSON converts the value to a JSON string.
func (v *Value) ToJSON() string {
	if v.ctx == nil || v.ptr == nil {
		panic("Value or context were reset.")
	}
	str := C.PersistentToJSON(v.ctx.v8context, v.ptr)
	defer C.free(unsafe.Pointer(str))
	return C.GoString(str)
}

// ToString converts a value holding a JS String to a string.  If the value
// is not actually a string, this will return an error.
func (v *Value) ToString() (string, error) {
	if v.ctx == nil || v.ptr == nil {
		panic("Value or context were reset.")
	}
	var str string
	err := json.Unmarshal([]byte(v.ToJSON()), &str)
	return str, err
}

// Burst converts a value that represents a JS Object and returns a map of
// key -> Value for each of the object's fields.  If the value is not an
// Object, an error is returned.
func (v *Value) Burst() (map[string]*Value, error) {
	if v.ctx == nil || v.ptr == nil {
		panic("Value or context were reset.")
	}
	// Call cgo to burst the object, get a list of KeyValuePairs back.
	var numKeys C.int
	keyValuesPtr := C.v8_BurstPersistent(v.ctx.v8context, v.ptr, &numKeys)

	if keyValuesPtr == nil {
		err := C.v8_error(v.ctx.v8context)
		defer C.free(unsafe.Pointer(err))
		return nil, errors.New(C.GoString(err) + ":" + v.ToJSON())
	}

	// Convert the list to a slice:
	var keyValues []C.struct_KeyValuePair
	sliceHeader := (*reflect.SliceHeader)((unsafe.Pointer(&keyValues)))
	sliceHeader.Cap = int(numKeys)
	sliceHeader.Len = int(numKeys)
	sliceHeader.Data = uintptr(unsafe.Pointer(keyValuesPtr))

	// Create the object map:
	result := make(map[string]*Value)
	for _, keyVal := range keyValues {
		key := C.GoString(keyVal.keyName)
		val := v.ctx.newValue(keyVal.value)

		result[key] = val

		// Don't forget to clean up!
		C.free(unsafe.Pointer(keyVal.keyName))
	}
	return result, nil
}

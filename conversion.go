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

	// Right now we mostly cast straight over to
	// longlong/ulonglong but in the future we might be able
	// to support more precise data types on the v8
	// side
	switch val.(type) {
	case int:
		prim, _ := val.(int)
		ret := C.v8_create_integer(v.v8context, C.longlong(prim))
		return v.newValue(ret), nil
	case int8:
		prim, _ := val.(int8)
		ret := C.v8_create_integer(v.v8context, C.longlong(prim))
		return v.newValue(ret), nil
	case int16:
		prim, _ := val.(int16)
		ret := C.v8_create_integer(v.v8context, C.longlong(prim))
		return v.newValue(ret), nil
	case int32:
		prim, _ := val.(int32)
		ret := C.v8_create_integer(v.v8context, C.longlong(prim))
		return v.newValue(ret), nil
	case int64:
		prim, _ := val.(int64)
		ret := C.v8_create_integer(v.v8context, C.longlong(prim))
		return v.newValue(ret), nil
	case uint8:
		prim, _ := val.(uint8)
		ret := C.v8_create_unsigned_integer(v.v8context, C.ulonglong(prim))
		return v.newValue(ret), nil
	case uint16:
		prim, _ := val.(uint16)
		ret := C.v8_create_unsigned_integer(v.v8context, C.ulonglong(prim))
		return v.newValue(ret), nil
	case uint32:
		prim, _ := val.(uint32)
		ret := C.v8_create_unsigned_integer(v.v8context, C.ulonglong(prim))
		return v.newValue(ret), nil
	case uint64:
		prim, _ := val.(uint64)
		ret := C.v8_create_unsigned_integer(v.v8context, C.ulonglong(prim))
		return v.newValue(ret), nil
	case uint:
		prim, _ := val.(uint)
		ret := C.v8_create_unsigned_integer(v.v8context, C.ulonglong(prim))
		return v.newValue(ret), nil
	case float32:
		prim, _ := val.(float32)
		ret := C.v8_create_float(v.v8context, C.float(prim))
		return v.newValue(ret), nil
	case float64:
		prim, _ := val.(float64)
		ret := C.v8_create_double(v.v8context, C.double(prim))
		return v.newValue(ret), nil
	case bool:
		prim, _ := val.(bool)
		ret := C.v8_create_bool(v.v8context, C.bool(prim))
		return v.newValue(ret), nil
	case string:
		prim, _ := val.(string)
		ret := C.v8_create_string(v.v8context, C.CString(prim))
		return v.newValue(ret), nil
	}

	data, err := json.Marshal(val)
	if err != nil {
		return nil, fmt.Errorf("Cannot marshal value as JSON: %v\nVal: %#v", err, val)
	}
	return v.FromJSON(string(data))
}

// TryConvertInternal will attempt to return a native go interface
// from a given Value. This will only succeed if the Value was originally
// created from an instantiated local prototype
func (v *V8Context) TryConvertInternal(value *Value) (interface{}, error) {
	if value == nil {
		return nil, fmt.Errorf("Value cannot be null")
	}
	if value.ctx != v {
		return nil, fmt.Errorf("Context mismatch")
	}
	ret := C.v8_get_internal_pointer(v.v8context, value.ptr)
	if ret == nil {
		err := C.v8_error(v.v8context)
		defer C.free(unsafe.Pointer(err))
		return nil, fmt.Errorf("Failed to convert internal value: %s", C.GoString(err))
	}
	wrapped := *(*V8Wrapped)(unsafe.Pointer(ret))
	return wrapped.internal, nil
}

// ToInt8 Will attempt to convert a V8 value to a native int8
func (v *Value) ToInt8() (int8, error) {
	var result C.long

	ret := C.v8_convert_to_int(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return 0, fmt.Errorf("Failed to convert value")
	}
	return int8(result), nil
}

// ToInt16 Will attempt to convert a V8 value to a native int16
func (v *Value) ToInt16() (int16, error) {
	var result C.long

	ret := C.v8_convert_to_int(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return 0, fmt.Errorf("Failed to convert value")
	}
	return int16(result), nil
}

// ToInt32 Will attempt to convert a V8 value to a native int32
func (v *Value) ToInt32() (int32, error) {
	var result C.long

	ret := C.v8_convert_to_int(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return 0, fmt.Errorf("Failed to convert value")
	}
	return int32(result), nil
}

// ToInt64 Will attempt to convert a V8 value to a native int64
func (v *Value) ToInt64() (int64, error) {
	var result C.longlong

	ret := C.v8_convert_to_int64(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return 0, fmt.Errorf("Failed to convert value")
	}
	return int64(result), nil
}

// ToUint8 Will attempt to convert a V8 value to a native uint8
func (v *Value) ToUint8() (uint8, error) {
	var result C.ulonglong

	ret := C.v8_convert_to_uint(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return 0, fmt.Errorf("Failed to convert value")
	}
	return uint8(result), nil
}

// ToUint16 Will attempt to convert a V8 value to a native uint16
func (v *Value) ToUint16() (uint16, error) {
	var result C.ulonglong

	ret := C.v8_convert_to_uint(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return 0, fmt.Errorf("Failed to convert value")
	}
	return uint16(result), nil
}

// ToUint32 Will attempt to convert a V8 value to a native uin32
func (v *Value) ToUint32() (uint32, error) {
	var result C.ulonglong

	ret := C.v8_convert_to_uint(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return 0, fmt.Errorf("Failed to convert value")
	}
	return uint32(result), nil
}

// ToUint64 Will attempt to convert a V8 value to a native uint64
func (v *Value) ToUint64() (uint64, error) {
	var result C.ulonglong

	ret := C.v8_convert_to_uint(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return 0, fmt.Errorf("Failed to convert value")
	}
	return uint64(result), nil
}

// ToFloat32 Will attempt to convert a V8 value to a native float32
func (v *Value) ToFloat32() (float32, error) {
	var result C.float

	ret := C.v8_convert_to_float(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return 0, fmt.Errorf("Failed to convert value")
	}
	return float32(result), nil
}

// ToFloat64 Will attempt to convert a V8 value to a native float64
func (v *Value) ToFloat64() (float64, error) {
	var result C.double

	ret := C.v8_convert_to_double(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return 0, fmt.Errorf("Failed to convert value")
	}
	return float64(result), nil
}

// ToBool Will attempt to convert a V8 value to a native bool
func (v *Value) ToBool() (bool, error) {
	var result C.bool

	ret := C.v8_convert_to_bool(v.ctx.v8context, v.ptr, &result)
	if ret == false {
		return false, fmt.Errorf("Failed to convert value")
	}
	return bool(result), nil
}

// ToBytes Will attempt to convert a V8 value to a native byte array
// NOTE: this does not copy the data
// NOTE: As a result of this array being backed by v8 memory
// the potential exist for the array to be released on the Go side
// and for v8 to dispose the backed data. If someone tries to use the
// byte array at that point the results are undefined (probably not good)
// TODO Figure out a better way to make this safe
func (v *Value) ToBytes() ([]byte, error) {
	var result *C.char
	var length C.int

	ret := C.v8_get_typed_backing(v.ctx.v8context, v.ptr, &result, &length)
	if ret == false {
		return nil, fmt.Errorf("Failed to convert value")
	}
	data := (*[1 << 30]byte)(unsafe.Pointer(result))[:length:length]
	return data, nil
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

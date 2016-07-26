#include "v8wrap.h"

#include "libplatform/libplatform.h"
#include "v8.h"
#include "v8context.h"
#include "v8isolate.h"

extern "C" PlatformPtr v8_init() {
  v8::Platform *platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  v8::V8::Initialize();
  std::string v8_flags = "--expose-gc --harmony-modules";
  v8::V8::SetFlagsFromString(v8_flags.c_str(),v8_flags.size());
  return (void *)platform;
}

extern "C" IsolatePtr v8_create_isolate() {
  return static_cast<IsolatePtr>(new V8Isolate());
}

extern "C" void v8_release_isolate(IsolatePtr isolate) {
  delete static_cast<V8Isolate *>(isolate);
}

extern "C" ContextPtr v8_create_context(IsolatePtr isolate, uint32_t id) {
  return static_cast<ContextPtr>(
      static_cast<V8Isolate *>(isolate)->MakeContext(id));
}

extern "C" void v8_release_context(ContextPtr ctx) {
  delete static_cast<V8Context *>(ctx);
}

extern "C" void v8_pump_loop(IsolatePtr isolate,PlatformPtr platform) {
  static_cast<V8Isolate *>(isolate)->PumpMessageLoop(static_cast<v8::Platform *>(platform));
}


extern "C" char *v8_execute(ContextPtr ctx, char *str, char *debugFilename) {
  return (static_cast<V8Context *>(ctx))->Execute(str, debugFilename);
}

extern "C" ObjectPtr v8_get_internal_pointer(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->GetInternalPointer(val);
}

extern "C" PersistentValuePtr v8_wrap(ContextPtr ctx, ObjectPtr identifier,PersistentValuePtr funcTemplate) {
  return (static_cast<V8Context *>(ctx))->Wrap(identifier,funcTemplate);
}

extern "C" PersistentValuePtr v8_create_object_prototype(ContextPtr ctx) {
  return (static_cast<V8Context *>(ctx))->CreateObjectPrototype();
}

extern "C" PersistentValuePtr v8_create_class_prototype(ContextPtr ctx, char *str) {
  return (static_cast<V8Context *>(ctx))->CreateClassPrototype(str);
}

extern "C" PersistentValuePtr v8_wrap_instance(ContextPtr ctx,ObjectPtr identifier, PersistentValuePtr instance) {
  return (static_cast<V8Context *>(ctx))->WrapInstance(identifier, instance);
}

extern "C" PersistentValuePtr v8_get_class_constructor(ContextPtr ctx, PersistentValuePtr funcTemplate) {
  return (static_cast<V8Context *>(ctx))->GetClassConstructor(funcTemplate);
}

 extern "C" void v8_add_method(ContextPtr ctx, char *str, FuncPtr callback, PersistentValuePtr funcTemplate) {
   (static_cast<V8Context *>(ctx))->AddWrappedMethod(str, callback,funcTemplate);
 }

extern "C" PersistentValuePtr v8_create_integer(ContextPtr ctx, long long int val) {
  return (static_cast<V8Context *>(ctx))->CreateInteger(val);
}

extern "C" PersistentValuePtr v8_create_unsigned_integer(ContextPtr ctx, long long unsigned int val) {
  return (static_cast<V8Context *>(ctx))->CreateUnsignedInteger(val);
}

extern "C" PersistentValuePtr v8_create_float(ContextPtr ctx, float val) {
  return (static_cast<V8Context *>(ctx))->CreateFloat(val);
}

extern "C" PersistentValuePtr v8_create_double(ContextPtr ctx, double val) {
  return (static_cast<V8Context *>(ctx))->CreateDouble(val);
}

extern "C" PersistentValuePtr v8_create_object_array(ContextPtr ctx, PersistentValuePtr *ptrs, int length) {
  return (static_cast<V8Context *>(ctx))->CreateObjectArray(ptrs, length);
}


extern "C" PersistentValuePtr v8_create_bool(ContextPtr ctx, bool val) {
  return (static_cast<V8Context *>(ctx))->CreateBool(val);
}

extern "C" PersistentValuePtr v8_create_string(ContextPtr ctx, char *val) {
  return (static_cast<V8Context *>(ctx))->CreateString(val);
}

extern "C" PersistentValuePtr v8_create_undefined(ContextPtr ctx) {
  return (static_cast<V8Context *>(ctx))->CreateUndefined();
}

extern "C" PersistentValuePtr v8_create_null(ContextPtr ctx) {
  return (static_cast<V8Context *>(ctx))->CreateNull();
}


extern "C" bool v8_convert_to_int64(ContextPtr ctx, PersistentValuePtr val, long long int* out) {
  return (static_cast<V8Context *>(ctx))->ConvertToInt64(val,out);
}

extern "C" bool v8_convert_to_int(ContextPtr ctx, PersistentValuePtr val, long int* out) {
  return (static_cast<V8Context *>(ctx))->ConvertToInt(val,out);
}

extern "C" bool v8_convert_to_uint(ContextPtr ctx, PersistentValuePtr val, long long unsigned int* out) {
  return (static_cast<V8Context *>(ctx))->ConvertToUint(val,out);
}

extern "C" bool v8_convert_to_float(ContextPtr ctx, PersistentValuePtr val, float* out) {
  return (static_cast<V8Context *>(ctx))->ConvertToFloat(val,out);
}

extern "C" bool v8_convert_to_double(ContextPtr ctx, PersistentValuePtr val, double* out) {
  return (static_cast<V8Context *>(ctx))->ConvertToDouble(val,out);
}

extern "C" bool v8_convert_to_bool(ContextPtr ctx, PersistentValuePtr val, bool* out) {
  return (static_cast<V8Context *>(ctx))->ConvertToBool(val,out);
}

extern "C" bool v8_get_typed_backing(ContextPtr ctx, PersistentValuePtr val, char** out, int* length) {
  return (static_cast<V8Context *>(ctx))->GetTypedArrayBacking(val,out,length);
}


extern "C" bool v8_is_array_buffer(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->IsArrayBuffer(val);
}
extern "C" bool v8_is_data_view(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->IsDataView(val);
}
extern "C" bool v8_is_date(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->IsDate(val);
}
extern "C" bool v8_is_map(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->IsMap(val);
}
extern "C" bool v8_is_map_iterator(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->IsMapIterator(val);
}
extern "C" bool v8_is_promise(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->IsPromise(val);
}
extern "C" bool v8_is_regexp(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->IsRegExp(val);
}
extern "C" bool v8_is_set(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->IsSet(val);
}
extern "C" bool v8_is_set_iterator(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->IsSetIterator(val);
}
extern "C" bool v8_is_typed_array(ContextPtr ctx, PersistentValuePtr val) {
  return (static_cast<V8Context *>(ctx))->IsTypedArray(val);
}


extern "C" PersistentValuePtr v8_eval(ContextPtr ctx, char *str,
                                      char *debugFilename) {
  return (static_cast<V8Context *>(ctx))->Eval(str, debugFilename);
}

extern "C" PersistentValuePtr v8_eval_with_context(ContextPtr ctx, char *str,
                                      char *debugFilename, int lineNumber, int column) {
  return (static_cast<V8Context *>(ctx))->EvalWithContext(str, debugFilename, lineNumber,column);
}

extern "C" PersistentValuePtr v8_compile_run_module(ContextPtr ctx, char *str,const char* filename) {
  return (static_cast<V8Context *>(ctx))->CompileRunModule(str,filename);
}

extern "C" PersistentValuePtr v8_apply(ContextPtr ctx, PersistentValuePtr func,
                                       PersistentValuePtr self, int argc,
                                       PersistentValuePtr *argv) {
  return (static_cast<V8Context *>(ctx))->Apply(func, self, argc, argv);
}

extern "C" char *PersistentToJSON(ContextPtr ctx,
                                  PersistentValuePtr persistent) {
  return (static_cast<V8Context *>(ctx))->PersistentToJSON(persistent);
}

extern "C" void *v8_BurstPersistent(ContextPtr ctx,
                                    PersistentValuePtr persistent,
                                    int *out_numKeys) {
  return (void *)((static_cast<V8Context *>(ctx))
                      ->BurstPersistent(persistent, out_numKeys));
}

extern "C" const char *v8_setPersistentField(ContextPtr ctx,
                                             PersistentValuePtr persistent,
                                             const char *field,
                                             PersistentValuePtr value) {
  return ((static_cast<V8Context *>(ctx))
              ->SetPersistentField(persistent, field, value));
}

extern "C" void v8_release_persistent(ContextPtr ctx,
                                      PersistentValuePtr persistent) {
  (static_cast<V8Context *>(ctx))->ReleasePersistent(persistent);
}
extern "C" void v8_weaken_persistent(ContextPtr ctx,
                                      PersistentValuePtr persistent) {
  (static_cast<V8Context *>(ctx))->WeakenPersistent(persistent);
}

extern "C" char *v8_error(ContextPtr ctx) {
  return (static_cast<V8Context *>(ctx))->Error();
}

extern "C" void v8_throw(ContextPtr ctx, char *errmsg) {
  return (static_cast<V8Context *>(ctx))->Throw(errmsg);
}

extern "C" void v8_terminate(IsolatePtr isolate) {
  (static_cast<V8Isolate *>(isolate))->Terminate();
}

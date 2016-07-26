#ifndef V8WRAP_H
#define V8WRAP_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef void *IsolatePtr;
typedef void *ContextPtr;
typedef void *PersistentValuePtr;
typedef void *PlatformPtr;
typedef long long unsigned int FuncPtr;
typedef long long unsigned int ObjectPtr;


extern PlatformPtr v8_init();

extern IsolatePtr v8_create_isolate();

extern void v8_release_isolate(IsolatePtr isolate);

extern ContextPtr v8_create_context(IsolatePtr isolate, uint32_t id);

extern void v8_release_context(ContextPtr ctx);

extern void v8_pump_loop(IsolatePtr isolate,PlatformPtr platform);

extern char *v8_execute(ContextPtr ctx, char *str, char *debugFilename);

extern PersistentValuePtr v8_eval(ContextPtr ctx, char *str,
                                  char *debugFilename);

extern PersistentValuePtr v8_eval_with_context(ContextPtr ctx, char *str,
                                  char *debugFilename, int line, int column);

extern PersistentValuePtr v8_compile_run_module(ContextPtr ctx, char *str,const char* filename);

extern PersistentValuePtr v8_apply(ContextPtr ctx, PersistentValuePtr func,
                                   PersistentValuePtr self, int argc,
                                   PersistentValuePtr *argv);

extern PersistentValuePtr v8_create_object_prototype(ContextPtr ctx);
extern PersistentValuePtr v8_create_class_prototype(ContextPtr ctx,char *str);
extern PersistentValuePtr v8_get_class_constructor(ContextPtr ctx, PersistentValuePtr funcTemplate);
extern PersistentValuePtr v8_wrap_instance(ContextPtr ctx, ObjectPtr identifier, PersistentValuePtr instance);
extern ObjectPtr v8_get_internal_pointer(ContextPtr ctx,PersistentValuePtr val);
extern void v8_add_method(ContextPtr ctx, char *str, FuncPtr callback, PersistentValuePtr funcTemplate);
extern PersistentValuePtr v8_wrap(ContextPtr ctx, ObjectPtr identifier, PersistentValuePtr funcTemplate);

extern PersistentValuePtr v8_create_integer(ContextPtr ctx, long long int value);
extern PersistentValuePtr v8_create_unsigned_integer(ContextPtr ctx, long long unsigned int value);
extern PersistentValuePtr v8_create_float(ContextPtr ctx, float value);
extern PersistentValuePtr v8_create_double(ContextPtr ctx, double value);
extern PersistentValuePtr v8_create_bool(ContextPtr ctx, bool value);
extern PersistentValuePtr v8_create_string(ContextPtr ctx, char *value);
extern PersistentValuePtr v8_create_undefined(ContextPtr ctx);
extern PersistentValuePtr v8_create_null(ContextPtr ctx);
extern PersistentValuePtr v8_create_object_array(ContextPtr ctx, PersistentValuePtr *ptrs, int length);


extern bool v8_convert_to_int(ContextPtr ctx, PersistentValuePtr, long int* out);
extern bool v8_convert_to_int64(ContextPtr ctx, PersistentValuePtr, long long int* out);
extern bool v8_convert_to_uint(ContextPtr ctx, PersistentValuePtr, long long unsigned int* out);
extern bool v8_convert_to_float(ContextPtr ctx, PersistentValuePtr, float* out);
extern bool v8_convert_to_double(ContextPtr ctx, PersistentValuePtr, double* out);
extern bool v8_convert_to_bool(ContextPtr ctx, PersistentValuePtr, bool* out);

extern bool v8_get_typed_backing(ContextPtr ctx, PersistentValuePtr, char** out, int* length);


extern bool v8_is_array_buffer(ContextPtr ctx, PersistentValuePtr val);
extern bool v8_is_data_view(ContextPtr ctx, PersistentValuePtr val);
extern bool v8_is_date(ContextPtr ctx, PersistentValuePtr val);
extern bool v8_is_map(ContextPtr ctx, PersistentValuePtr val);
extern bool v8_is_map_iterator(ContextPtr ctx, PersistentValuePtr val);
extern bool v8_is_promise(ContextPtr ctx, PersistentValuePtr val);
extern bool v8_is_regexp(ContextPtr ctx, PersistentValuePtr val);
extern bool v8_is_set(ContextPtr ctx, PersistentValuePtr val);
extern bool v8_is_set_iterator(ContextPtr ctx, PersistentValuePtr val);
extern bool v8_is_typed_array(ContextPtr ctx, PersistentValuePtr val);


extern char *PersistentToJSON(ContextPtr ctx, PersistentValuePtr persistent);

struct KeyValuePair {
  char *keyName;
  PersistentValuePtr value;
};

// Returns NULL on errors, otherwise allocates an array of KeyValuePairs
// and sets out_numkeys to the length.
// For some reason, cgo barfs if the return type is KeyValuePair*, so we
// return a void* and it's cast back to KeyValuePair* on the other side.
extern void *v8_BurstPersistent(ContextPtr ctx, PersistentValuePtr persistent,
                                int *out_numKeys);

// Returns a constant error string on errors, otherwise a NULL.  The error msg
// should NOT be freed by the caller.
extern const char *v8_setPersistentField(ContextPtr ctx,
                                         PersistentValuePtr persistent,
                                         const char *field,
                                         PersistentValuePtr value);

extern void v8_release_persistent(ContextPtr ctx,
                                  PersistentValuePtr persistent);

extern void v8_weaken_persistent(ContextPtr ctx,
                                  PersistentValuePtr persistent);

extern char *v8_error(ContextPtr ctx);

extern void v8_throw(ContextPtr ctx, char *errmsg);

extern void v8_terminate(IsolatePtr iso);

#ifdef __cplusplus
}
#endif

#endif  // !defined(V8WRAP_H)

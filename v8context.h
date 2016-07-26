#ifndef V8CONTEXT_H
#define V8CONTEXT_H

#include <string>

#include "v8.h"
#include "v8wrap.h"

class V8Context {
 public:
  V8Context(v8::Isolate* isolate,uint32_t id);
  ~V8Context();

  char* Execute(const char* source, const char* filename);
  char* Error();


  PersistentValuePtr CreateString(const char* val);
  PersistentValuePtr CreateBool(bool val);
  PersistentValuePtr CreateInteger(long long int val);
  PersistentValuePtr CreateUnsignedInteger(long long unsigned int val);
  PersistentValuePtr CreateFloat(float val);
  PersistentValuePtr CreateDouble(double val);
  PersistentValuePtr CreateNull();
  PersistentValuePtr CreateUndefined();
  PersistentValuePtr CreateObjectArray(PersistentValuePtr *ptrs, int length);


  bool ConvertToInt(PersistentValuePtr val, long int* out);
  bool ConvertToInt64(PersistentValuePtr val, long long int* out);
  bool ConvertToUint(PersistentValuePtr val, long long unsigned int* out);
  bool ConvertToFloat(PersistentValuePtr val, float* out);
  bool ConvertToDouble(PersistentValuePtr val, double* out);
  bool ConvertToBool(PersistentValuePtr val, bool* out);
  bool GetTypedArrayBacking(PersistentValuePtr val, char** out, int* length);

  bool IsArrayBuffer(PersistentValuePtr val);
  bool IsDataView(PersistentValuePtr val);
  bool IsDate(PersistentValuePtr val);
  bool IsMap(PersistentValuePtr val);
  bool IsMapIterator(PersistentValuePtr val);
  bool IsPromise(PersistentValuePtr val);
  bool IsRegExp(PersistentValuePtr val);
  bool IsSetIterator(PersistentValuePtr val);
  bool IsSet(PersistentValuePtr val);
  bool IsTypedArray(PersistentValuePtr val);



  PersistentValuePtr Eval(const char* str, const char* debugFilename);
  PersistentValuePtr EvalWithContext(const char* str, const char* debugFilename, int lineNumber, int column);
  PersistentValuePtr CompileRunModule(const char* str,const char* filename);

  PersistentValuePtr CreateObjectPrototype();
  PersistentValuePtr CreateClassPrototype(const char* str);
    PersistentValuePtr GetClassConstructor(PersistentValuePtr funcTemplate);
    
  void AddWrappedMethod(const char* name, FuncPtr callback, PersistentValuePtr funcTemplate);

  PersistentValuePtr Wrap(ObjectPtr identifier, PersistentValuePtr funcTemplate);
  PersistentValuePtr WrapInstance(ObjectPtr identifier, PersistentValuePtr instance);

  ObjectPtr GetInternalPointer(PersistentValuePtr val);

  PersistentValuePtr Apply(PersistentValuePtr func, PersistentValuePtr self,
                           int argc, PersistentValuePtr* argv);

  char* PersistentToJSON(PersistentValuePtr persistent);

  void ReleasePersistent(PersistentValuePtr persistent);
  void WeakenPersistent(PersistentValuePtr persistent);
  KeyValuePair* BurstPersistent(PersistentValuePtr persistent,
                                int* out_numKeys);

  // Returns an error message on failure, otherwise returns NULL.
  const char* SetPersistentField(PersistentValuePtr persistent,
                                 const char* field, PersistentValuePtr value);

  void Throw(const char* errmsg);
  v8::Local<v8::Context> GetContext();
 private:
  std::string report_exception(v8::TryCatch& try_catch);
  uint32_t  mId;

  v8::Isolate* mIsolate;
  v8::Persistent<v8::Context> mContext;
  std::string mLastError;
};

#endif  // !defined(V8CONTEXT_H)

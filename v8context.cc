#include "v8context.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

#include <iostream>

extern "C" char* _go_v8_callback(unsigned int ctxID, char* name, char* args);

extern "C" char* _go_v8_dispose_wrapped(ObjectPtr identifier);

extern "C" PersistentValuePtr _go_v8_callback_raw(
    unsigned int ctxID, const char* name, const char* callerFuncname,
    const char* callerFilename, int callerLine, int callerColumn, int argc,
    PersistentValuePtr* argv);

    extern "C" PersistentValuePtr _go_v8_construct_wrapped(
        unsigned int ctxID, const char* name, const char* callerFuncname,
        const char* callerFilename, int callerLine, int callerColumn, int argc,
        PersistentValuePtr* argv);

extern "C" PersistentValuePtr _go_v8_callback_wrapped(ObjectPtr identifier, FuncPtr fn, const char* callerFuncname,
    const char* callerFilename, int callerLine, int callerColumn, int argc,
    PersistentValuePtr* argv);

extern "C" PersistentValuePtr _go_v8_property_get (
    ObjectPtr identifier, const char* name);

namespace {

// Calling JSON.stringify on value.
std::string to_json(v8::Isolate* iso, v8::Local<v8::Value> value) {
  v8::HandleScope scope(iso);
  v8::TryCatch try_catch;
  v8::Local<v8::Object> json =
      v8::Local<v8::Object>::Cast(iso->GetCurrentContext()->Global()->Get(
          v8::String::NewFromUtf8(iso, "JSON")));
  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(
      json->GetRealNamedProperty(v8::String::NewFromUtf8(iso, "stringify")));
  v8::Local<v8::Value> args[1];
  args[0] = value;
  v8::String::Utf8Value ret(
      func->Call(iso->GetCurrentContext()->Global(), 1, args)->ToString());
  return *ret;
}

// Calling JSON.parse on str.
v8::Local<v8::Value> from_json(v8::Isolate* iso, std::string str) {
  v8::HandleScope scope(iso);
  v8::TryCatch try_catch;
  v8::Local<v8::Object> json =
      v8::Local<v8::Object>::Cast(iso->GetCurrentContext()->Global()->Get(
          v8::String::NewFromUtf8(iso, "JSON")));
  v8::Local<v8::Function> func = v8::Local<v8::Function>::Cast(
      json->GetRealNamedProperty(v8::String::NewFromUtf8(iso, "parse")));
  v8::Local<v8::Value> args[1];
  args[0] = v8::String::NewFromUtf8(iso, str.c_str());
  return func->Call(iso->GetCurrentContext()->Global(), 1, args);
}

// _go_call is a helper function to call Go functions from within v8.
void _go_call(const v8::FunctionCallbackInfo<v8::Value>& args) {
  uint32_t id = args[0]->ToUint32()->Value();
  v8::String::Utf8Value name(args[1]);
  v8::String::Utf8Value argv(args[2]);
  v8::Isolate* iso = args.GetIsolate();
  v8::HandleScope scope(iso);
  v8::ReturnValue<v8::Value> ret = args.GetReturnValue();
  char* retv = _go_v8_callback(id, *name, *argv);
  if (retv != NULL) {
    ret.Set(from_json(iso, retv));
    free(retv);
  }
}

std::string str(v8::Local<v8::Value> value) {
  v8::String::Utf8Value s(value);
  if (s.length() == 0) {
    return "";
  }
  return *s;
}

static void WeakCallback(const v8::WeakCallbackData<v8::Object, void>& data) {
      v8::Isolate* iso = data.GetIsolate();
      v8::HandleScope scope(iso);

      ObjectPtr identifier = reinterpret_cast<long long unsigned int>(data.GetParameter());

      _go_v8_dispose_wrapped(identifier);
}

static void WeakStandaloneCallback(const v8::WeakCallbackData<v8::Value, void>& data) {
      v8::Isolate* iso = data.GetIsolate();
      v8::HandleScope scope(iso);

      v8::Persistent<v8::Value>* persist =
          static_cast<v8::Persistent<v8::Value>*>(data.GetParameter());
          persist->Reset();
}


static void GlobalPropertyGetterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& args) {
      v8::Isolate* iso = args.GetIsolate();
      v8::HandleScope scope(iso);

    //  v8::Local<v8::External> idWrap = v8::Local<v8::External>::Cast(args.Data());
    //  ObjectPtr identifier = reinterpret_cast<long long unsigned int>(idWrap->Value());
    v8::Local<v8::External> idWrap = v8::Local<v8::External>::Cast(args.Holder()->GetInternalField(0));
    ObjectPtr identifier = reinterpret_cast<long long unsigned int>(idWrap->Value());

      v8::String::Utf8Value prop(property);
      PersistentValuePtr retv = _go_v8_property_get(identifier,*prop);

      if (retv == NULL) {
        v8::MaybeLocal<v8::Value> maybe_rv = args.Holder()->GetRealNamedProperty(iso->GetCurrentContext(),property);
      //  std::cout << std::endl << "Holder context " /*<< iso->GetCurrentContext()*/ << std::endl;
        if (maybe_rv.IsEmpty()) {
        /*  v8::Local<v8::External> contextWrap = v8::Local<v8::External>::Cast(args.Holder()->GetInternalField(1));
          V8Context* context = reinterpret_cast<V8Context*>(contextWrap->Value());
          //std::cout << std::endl << "Internal context " << std::endl;
        //  std::cout << std::endl << "Internal context == " << (context->GetContext() == iso->GetCurrentContext()) << std::endl;
          maybe_rv = args.Holder()->GetRealNamedProperty(context->GetContext(),property);
          if (maybe_rv.IsEmpty()) {*/
          //  std::cout << std::endl << "Empty Empty " << std::endl;
            args.GetReturnValue().Set(v8::Undefined(iso));
            return;
          /*}*/
        }

        v8::Local<v8::Value> rv;
        if (maybe_rv.ToLocal(&rv)) {
          args.GetReturnValue().Set(rv);
          return;
        }

      } else {
        args.GetReturnValue().Set(*static_cast<v8::Persistent<v8::Value>*>(retv));
      }
}


static void GlobalPropertySetterCallback(
    v8::Local<v8::Name> property,
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<v8::Value>& args) {

}


static void GlobalPropertyQueryCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Integer>& args) {
}


static void GlobalPropertyDeleterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Boolean>& args) {

}


static void GlobalPropertyEnumeratorCallback(
    const v8::PropertyCallbackInfo<v8::Array>& args) {
}

// _go_call_raw is a helper function to call Go functions from within v8.
void _go_call_raw(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* iso = args.GetIsolate();
  v8::HandleScope scope(iso);

  uint32_t id = args[0]->ToUint32()->Value();
  v8::String::Utf8Value name(args[1]);
  v8::Local<v8::Array> hargv = v8::Local<v8::Array>::Cast(args[2]);

  std::string src_file, src_func;
  int line_number = 0, column = 0;
  v8::Local<v8::StackTrace> trace(v8::StackTrace::CurrentStackTrace(iso, 2));
  if (trace->GetFrameCount() == 2) {
    v8::Local<v8::StackFrame> frame(trace->GetFrame(1));
    src_file = str(frame->GetScriptName());
    src_func = str(frame->GetFunctionName());
    line_number = frame->GetLineNumber();
    column = frame->GetColumn();
  }

  int argc = hargv->Length();
  PersistentValuePtr argv[argc];
  for (int i = 0; i < argc; i++) {
    argv[i] = new v8::Persistent<v8::Value>(iso, hargv->Get(i));
  }

  PersistentValuePtr retv =
      _go_v8_callback_raw(id, *name, src_func.c_str(), src_file.c_str(),
                          line_number, column, argc, argv);

  if (retv == NULL) {
    args.GetReturnValue().Set(v8::Undefined(iso));
  } else {
    args.GetReturnValue().Set(*static_cast<v8::Persistent<v8::Value>*>(retv));
  }
}


void _go_call_wrapped(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* iso = args.GetIsolate();
  v8::HandleScope scope(iso);

  v8::Local<v8::External> idWrap = v8::Local<v8::External>::Cast(args.This()->GetInternalField(0));
  ObjectPtr identifier = reinterpret_cast<long long unsigned int>(idWrap->Value());


  v8::Local<v8::External> data = v8::Local<v8::External>::Cast(args.Data());
  FuncPtr fn = reinterpret_cast<long long unsigned int>(data->Value());


  std::string src_file, src_func;
  int line_number = 0, column = 0;
  v8::Local<v8::StackTrace> trace(v8::StackTrace::CurrentStackTrace(iso, 2));
  if (trace->GetFrameCount() == 2) {
    v8::Local<v8::StackFrame> frame(trace->GetFrame(1));
    src_file = str(frame->GetScriptName());
    src_func = str(frame->GetFunctionName());
    line_number = frame->GetLineNumber();
    column = frame->GetColumn();
  }

  int argc = args.Length();
  PersistentValuePtr argv[argc];
  for (int i = 0; i < argc; i++) {
    argv[i] = new v8::Persistent<v8::Value>(iso, args[i]);
  }

    PersistentValuePtr retv =
        _go_v8_callback_wrapped(identifier, fn, src_func.c_str(), src_file.c_str(),
                            line_number, column, argc, argv);

  if (retv == NULL) {
    args.GetReturnValue().Set(v8::Undefined(iso));
  } else {
    args.GetReturnValue().Set(*static_cast<v8::Persistent<v8::Value>*>(retv));
  }
}


void _go_call_construct_wrapped(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* iso = args.GetIsolate();
  v8::HandleScope scope(iso);

  v8::Local<v8::Array> data = v8::Local<v8::Array>::Cast(args.Data());
  v8::Local<v8::Value> idExtern = data->Get(0);
  v8::Local<v8::String> funcExtern = v8::Local<v8::String>::Cast(data->Get(1));
  uint32_t id = idExtern->ToUint32()->Value();
  v8::String::Utf8Value name(funcExtern);

  std::string src_file, src_func;
  int line_number = 0, column = 0;
  v8::Local<v8::StackTrace> trace(v8::StackTrace::CurrentStackTrace(iso, 2));
  if (trace->GetFrameCount() == 2) {
    v8::Local<v8::StackFrame> frame(trace->GetFrame(1));
    src_file = str(frame->GetScriptName());
    src_func = str(frame->GetFunctionName());
    line_number = frame->GetLineNumber();
    column = frame->GetColumn();
  }

  int argc = args.Length();
  PersistentValuePtr argv[argc+1];
  argv[0] = new v8::Persistent<v8::Value>(iso,args.This());
  for (int i = 0; i < argc; i++) {
    argv[i+1] = new v8::Persistent<v8::Value>(iso, args[i]);
  }

    PersistentValuePtr retv =
        _go_v8_construct_wrapped(id, *name, src_func.c_str(), src_file.c_str(),
                            line_number, column, argc+1, argv);

  if (retv == NULL) {
    args.GetReturnValue().Set(v8::Undefined(iso));
  } else {
    args.GetReturnValue().Set(*static_cast<v8::Persistent<v8::Value>*>(retv));
  }
}
};

V8Context::V8Context(v8::Isolate* isolate,uint32_t id) : mIsolate(isolate) {
  v8::Locker lock(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);

  v8::V8::SetCaptureStackTraceForUncaughtExceptions(true);

  v8::Local<v8::ObjectTemplate> globals = v8::ObjectTemplate::New(mIsolate);
  globals->Set(v8::String::NewFromUtf8(mIsolate, "_go_call"),
               v8::FunctionTemplate::New(mIsolate, _go_call));
  globals->Set(v8::String::NewFromUtf8(mIsolate, "_go_call_raw"),
               v8::FunctionTemplate::New(mIsolate, _go_call_raw));
  mId = id;
  mContext.Reset(mIsolate, v8::Context::New(mIsolate, NULL, globals));
};

V8Context::~V8Context() {
  v8::Locker lock(mIsolate);
  mContext.Reset();
};

v8::Local<v8::Context> V8Context::GetContext() {
  return mContext.Get(mIsolate);
}

char* V8Context::Execute(const char* source, const char* filename) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);

  mLastError.clear();

  v8::Local<v8::Script> script = v8::Script::Compile(
      v8::String::NewFromUtf8(mIsolate, source),
      v8::String::NewFromUtf8(mIsolate, filename ? filename : "undefined"));

  if (script.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  v8::Local<v8::Value> result = script->Run();

  if (result.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  if (result->IsFunction() || result->IsUndefined()) {
    return strdup("");
  } else {
    return strdup(to_json(mIsolate, result).c_str());
  }
}

PersistentValuePtr V8Context::Eval(const char* source, const char* filename) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);

  mLastError.clear();

  v8::Local<v8::Script> script = v8::Script::Compile(
      v8::String::NewFromUtf8(mIsolate, source),
      filename ? v8::String::NewFromUtf8(mIsolate, filename)
               : v8::String::NewFromUtf8(mIsolate, "undefined"));

  if (script.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  v8::Local<v8::Value> result = script->Run();

  if (result.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  return new v8::Persistent<v8::Value>(mIsolate, result);
}

PersistentValuePtr V8Context::EvalWithContext(const char* source, const char* filename, int linenumber, int column) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);

  mLastError.clear();
  v8::ScriptOrigin origin(v8::String::NewFromUtf8(mIsolate, filename),v8::Integer::New(mIsolate,linenumber),v8::Integer::New(mIsolate,column));
  // TODO Right now we're not caching this (maybe we should?)
  v8::ScriptCompiler::Source scriptsource(v8::String::NewFromUtf8(mIsolate, source),origin,NULL);
  v8::Local<v8::Script> script = v8::ScriptCompiler::Compile(mIsolate,&scriptsource);


  if (script.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  v8::Local<v8::Value> result = script->Run();

  if (result.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  return new v8::Persistent<v8::Value>(mIsolate, result);
}

PersistentValuePtr V8Context::CompileRunModule(const char* source,const char* filename) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);

  mLastError.clear();
  v8::ScriptOrigin origin(v8::String::NewFromUtf8(mIsolate, source));
  v8::ScriptCompiler::Source script_source(v8::String::NewFromUtf8(mIsolate, source), origin);
  v8::MaybeLocal<v8::Script> maybeScript = v8::ScriptCompiler::CompileModule(mContext.Get(mIsolate),
      &script_source);
  v8::Local<v8::Script> script;
  if (!maybeScript.ToLocal(&script)) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  v8::Local<v8::Value> result = script->Run();

  if (result.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  return new v8::Persistent<v8::Value>(mIsolate, result);
}

PersistentValuePtr V8Context::Apply(PersistentValuePtr func,
                                    PersistentValuePtr self, int argc,
                                    PersistentValuePtr* argv) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);

  mLastError.clear();

  v8::Local<v8::Value> pfunc =
      static_cast<v8::Persistent<v8::Value>*>(func)->Get(mIsolate);
  v8::Local<v8::Function> vfunc = v8::Local<v8::Function>::Cast(pfunc);

  v8::Local<v8::Value>* vargs = new v8::Local<v8::Value>[argc];
  for (int i = 0; i < argc; i++) {
    vargs[i] = static_cast<v8::Persistent<v8::Value>*>(argv[i])->Get(mIsolate);
  }

  // Global scope requested?
  v8::Local<v8::Object> vself;
  if (self == NULL) {
    vself = mContext.Get(mIsolate)->Global();
  } else {
    v8::Local<v8::Value> pself =
        static_cast<v8::Persistent<v8::Value>*>(self)->Get(mIsolate);
    vself = v8::Local<v8::Object>::Cast(pself);
  }

  v8::Local<v8::Value> result = vfunc->Call(vself, argc, vargs);

  delete[] vargs;

  if (result.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  return new v8::Persistent<v8::Value>(mIsolate, result);
}


ObjectPtr V8Context::GetInternalPointer(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);

  mLastError.clear();

  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);

  v8::Local<v8::Object> objectRef = v8::Local<v8::Object>::Cast(localVal);

  if (objectRef->InternalFieldCount() > 0) {
    v8::Local<v8::External> idWrap = v8::Local<v8::External>::Cast(objectRef->GetInternalField(0));
    ObjectPtr identifier = reinterpret_cast<long long unsigned int>(idWrap->Value());
    return identifier;
  }
  mLastError = "Value is not a wrapped internal";
  return 0;
}

PersistentValuePtr V8Context::CreateObjectPrototype() {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);

  v8::Local<v8::FunctionTemplate> function_template = v8::FunctionTemplate::New(mIsolate);
  function_template->SetHiddenPrototype(false);
  v8::Persistent<v8::FunctionTemplate>* ret = new v8::Persistent<v8::FunctionTemplate>(mIsolate, function_template);


  return ret;
}

PersistentValuePtr V8Context::CreateClassPrototype(const char* name) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);


  v8::Local<v8::Array> internals = v8::Array::New(mIsolate,2);
  internals->Set(0,v8::Uint32::New(mIsolate,mId));
  internals->Set(1,v8::String::NewFromUtf8(mIsolate, name));

  v8::Local<v8::FunctionTemplate> function_template = v8::FunctionTemplate::New(mIsolate,_go_call_construct_wrapped,internals);
  function_template->SetClassName(v8::String::NewFromUtf8(mIsolate, name));
  function_template->SetHiddenPrototype(false);
  function_template->InstanceTemplate()->SetInternalFieldCount(2);

  v8::NamedPropertyHandlerConfiguration config(GlobalPropertyGetterCallback/*,
                                           GlobalPropertySetterCallback,
                                           GlobalPropertyQueryCallback,
                                           GlobalPropertyDeleterCallback,
                                           GlobalPropertyEnumeratorCallback*/);


  function_template->InstanceTemplate()->SetHandler(config);

  v8::Persistent<v8::FunctionTemplate>* ret = new v8::Persistent<v8::FunctionTemplate>(mIsolate, function_template);


  return ret;
}

PersistentValuePtr V8Context::GetClassConstructor(PersistentValuePtr funcTemplate) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);


  v8::Local<v8::FunctionTemplate> function_template =
      static_cast<v8::Persistent<v8::FunctionTemplate>*>(funcTemplate)->Get(mIsolate);

  v8::Local<v8::Function> func = function_template->GetFunction();

  v8::Persistent<v8::Function>* ret = new v8::Persistent<v8::Function>(mIsolate, func);


  return ret;
}



void V8Context::AddWrappedMethod(const char* name, FuncPtr callback, PersistentValuePtr funcTemplate) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);

  v8::Local<v8::FunctionTemplate> function_template =
      static_cast<v8::Persistent<v8::FunctionTemplate>*>(funcTemplate)->Get(mIsolate);

  v8::Local<v8::Signature> signature = v8::Signature::New(mIsolate, function_template);
  v8::Local<v8::String> nameStr = v8::String::NewFromUtf8(mIsolate, name);
  v8::Local<v8::FunctionTemplate> proto = v8::FunctionTemplate::New(mIsolate, _go_call_wrapped, v8::External::New(mIsolate,reinterpret_cast<void*>(callback)), signature);

//std::cout << std::endl << callback << " " << reinterpret_cast<void*>(callback) << " " << reinterpret_cast<FuncPtr>(reinterpret_cast<void*>(callback)) << std::endl;
  v8::Local<v8::Template> proto_t = function_template->PrototypeTemplate();
  proto_t->Set(nameStr, proto);
  proto->SetClassName(nameStr);

}

PersistentValuePtr V8Context::WrapInstance(ObjectPtr identifier, PersistentValuePtr instance) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);

  mLastError.clear();


  v8::Local<v8::Object> v8Instance =
      static_cast<v8::Persistent<v8::Object>*>(instance)->Get(mIsolate);

      v8Instance->SetInternalField(0,v8::External::New(mIsolate,reinterpret_cast<void*>(identifier)));
      v8Instance->SetInternalField(1,v8::External::New(mIsolate,reinterpret_cast<void*>(this)));
  if (v8Instance.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  v8::Persistent<v8::Object>* ret = new v8::Persistent<v8::Object>(mIsolate, v8Instance);

  ret->SetWeak(reinterpret_cast<void*>(identifier), WeakCallback);
  ret->MarkIndependent();

  return ret;
}


PersistentValuePtr V8Context::Wrap(ObjectPtr identifier, PersistentValuePtr funcTemplate) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);

  mLastError.clear();


  v8::Local<v8::FunctionTemplate> function_template =
      static_cast<v8::Persistent<v8::FunctionTemplate>*>(funcTemplate)->Get(mIsolate);



      v8::Local<v8::ObjectTemplate> object_template =
          function_template->InstanceTemplate();
          object_template->SetInternalFieldCount(2);


      v8::NamedPropertyHandlerConfiguration config(GlobalPropertyGetterCallback/*,
                                               GlobalPropertySetterCallback,
                                               GlobalPropertyQueryCallback,
                                               GlobalPropertyDeleterCallback,
                                               GlobalPropertyEnumeratorCallback*/);

      //void* tmp = reinterpret_cast<void*>(identifier);
      //long long unsigned int tmp2 = reinterpret_cast<long long unsigned int>(tmp);

      //std::cout << std::endl << identifier << " " << (void*)&identifier << " " << tmp << " " << tmp2 << std::endl;

      config.data = v8::External::New(mIsolate,reinterpret_cast<void*>(identifier));

      object_template->SetHandler(config);
      v8::Local<v8::Object> result = object_template->NewInstance();
      result->SetInternalField(0,v8::External::New(mIsolate,reinterpret_cast<void*>(identifier)));
      result->SetInternalField(1,v8::External::New(mIsolate,reinterpret_cast<void*>(this)));
  if (result.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }

  v8::Persistent<v8::Object>* ret = new v8::Persistent<v8::Object>(mIsolate, result);

  ret->SetWeak(reinterpret_cast<void*>(identifier), WeakCallback);
  ret->MarkIndependent();

  return ret;
}

PersistentValuePtr V8Context::CreateObjectArray(PersistentValuePtr *ptrs, int length) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Array> result = v8::Array::New(mIsolate,length);
  for (int i; i < length; i++) {
    v8::Local<v8::Value> localVal =
         static_cast<v8::Persistent<v8::Value>*>(ptrs[i])->Get(mIsolate);
    result->Set(i,localVal);
  }
  return new v8::Persistent<v8::Value>(mIsolate, result);
}

PersistentValuePtr V8Context::CreateString(const char* val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> result = v8::String::NewFromUtf8(mIsolate,val);
  return new v8::Persistent<v8::Value>(mIsolate, result);
}

PersistentValuePtr V8Context::CreateInteger(long long int val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> result = v8::Integer::New(mIsolate,val);
  return new v8::Persistent<v8::Value>(mIsolate, result);
}

PersistentValuePtr V8Context::CreateUnsignedInteger(long long unsigned int val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> result = v8::Number::New(mIsolate,val);
  return new v8::Persistent<v8::Value>(mIsolate, result);
}

PersistentValuePtr V8Context::CreateBool(bool val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> result = v8::Boolean::New(mIsolate,val);
  return new v8::Persistent<v8::Value>(mIsolate, result);
}

PersistentValuePtr V8Context::CreateFloat(float val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::TryCatch try_catch;
  try_catch.SetVerbose(false);
    mLastError.clear();
  v8::Local<v8::Value> result = v8::Number::New(mIsolate,val);
  if (result.IsEmpty()) {
    mLastError = report_exception(try_catch);
    return NULL;
  }
  return new v8::Persistent<v8::Value>(mIsolate, result);
}

PersistentValuePtr V8Context::CreateDouble(double val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> result = v8::Number::New(mIsolate,val);
  return new v8::Persistent<v8::Value>(mIsolate, result);
}


PersistentValuePtr V8Context::CreateNull() {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> result = v8::Null(mIsolate);
  return new v8::Persistent<v8::Value>(mIsolate, result);

}

PersistentValuePtr V8Context::CreateUndefined() {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> result = v8::Undefined(mIsolate);
  return new v8::Persistent<v8::Value>(mIsolate, result);

}

bool V8Context::ConvertToInt64(PersistentValuePtr val, long long int* out) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));


  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  if (!localVal->IsNumber()) {
    return 0;
  }
  *out = localVal.As<v8::Integer>()->Value();
  return 1;

}

bool V8Context::ConvertToInt(PersistentValuePtr val, long int* out) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));


  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  if (!localVal->IsInt32()) {
    return 0;
  }
  *out = localVal.As<v8::Int32>()->Value();
  return 1;

}

bool V8Context::ConvertToUint(PersistentValuePtr val, long long unsigned int* out) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));


  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  if (!localVal->IsUint32()) {
    return 0;
  }
  *out = localVal.As<v8::Uint32>()->Value();
  return 1;

}

bool V8Context::ConvertToFloat(PersistentValuePtr val, float* out) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));


  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  if (!localVal->IsNumber()) {
    return 0;
  }
  *out = localVal.As<v8::Number>()->Value();
  return 1;

}

bool V8Context::ConvertToDouble(PersistentValuePtr val, double* out) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));


  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  if (!localVal->IsNumber()) {
    return 0;
  }
  *out = localVal.As<v8::Number>()->Value();
  return 1;

}

bool V8Context::ConvertToBool(PersistentValuePtr val, bool* out) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));


  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  if (!localVal->IsBoolean()) {
    return 0;
  }
  *out = localVal.As<v8::Boolean>()->Value();
  return 1;

}

bool V8Context::IsArrayBuffer(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  return localVal->IsArrayBuffer();
}

bool V8Context::IsDataView(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  return localVal->IsDataView();
}

bool V8Context::IsDate(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  return localVal->IsDate();
}

bool V8Context::IsMap(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  return localVal->IsMap();
}

bool V8Context::IsMapIterator(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  return localVal->IsMapIterator();
}

bool V8Context::IsPromise(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  return localVal->IsPromise();
}

bool V8Context::IsRegExp(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  return localVal->IsRegExp();
}

bool V8Context::IsSet(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  return localVal->IsSet();
}

bool V8Context::IsSetIterator(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  return localVal->IsSetIterator();
}

bool V8Context::IsTypedArray(PersistentValuePtr val) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  return localVal->IsTypedArray();
}

bool V8Context::GetTypedArrayBacking(PersistentValuePtr val, char** out, int* length) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));


  v8::Local<v8::Value> localVal =
       static_cast<v8::Persistent<v8::Value>*>(val)->Get(mIsolate);
  if (!localVal->IsUint8Array()) {
    return 0;
  }
  v8::Local<v8::Uint8Array> ui8 = localVal.As<v8::Uint8Array>();
  v8::ArrayBuffer::Contents ui8_c = ui8->Buffer()->GetContents();
  size_t ui8_length = ui8->ByteLength();
  size_t ui8_offset = ui8->ByteOffset();
  char* ui8_data = static_cast<char*>(ui8_c.Data()) + ui8_offset;
  *out = ui8_data;
  *length = ui8_length;
  return ui8_data;

}


char* V8Context::PersistentToJSON(PersistentValuePtr persistent) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> persist =
      static_cast<v8::Persistent<v8::Value>*>(persistent)->Get(mIsolate);
  return strdup(to_json(mIsolate, persist).c_str());
}

void V8Context::ReleasePersistent(PersistentValuePtr persistent) {
  v8::Locker locker(mIsolate);
  v8::Persistent<v8::Value>* persist =
      static_cast<v8::Persistent<v8::Value>*>(persistent);
  persist->Reset();
  delete persist;
}



void V8Context::WeakenPersistent(PersistentValuePtr persistent) {
  v8::Locker locker(mIsolate);
  v8::Persistent<v8::Value>* persist =
      static_cast<v8::Persistent<v8::Value>*>(persistent);
  persist->SetWeak(persistent, WeakStandaloneCallback);
  persist->MarkIndependent();
}

const char* V8Context::SetPersistentField(PersistentValuePtr persistent,
                                          const char* field,
                                          PersistentValuePtr value) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Persistent<v8::Value>* persist =
      static_cast<v8::Persistent<v8::Value>*>(persistent);
  v8::Local<v8::Value> name(v8::String::NewFromUtf8(mIsolate, field));

  // Create the local object now, but reset the persistent one later:
  // we could still fail setting the value, and then there is no point
  // in re-creating the persistent copy.
  v8::Local<v8::Value> maybeObject = persist->Get(mIsolate);
  if (!maybeObject->IsObject()) {
    return "The supplied receiver is not an object.";
  }

  // We can safely call `ToLocalChecked`, because
  // we've just created the local object above.
  v8::Local<v8::Object> object =
      maybeObject->ToObject(mContext.Get(mIsolate)).ToLocalChecked();

  v8::Persistent<v8::Value>* val =
      static_cast<v8::Persistent<v8::Value>*>(value);
  v8::Local<v8::Value> local_val = val->Get(mIsolate);

  if (!object->Set(name, local_val)) {
    return "Cannot set value";
  }

  // Now it is time to get rid of the previous persistent version,
  // and create a new one.
  persist->Reset(mIsolate, object);

  return NULL;
}

KeyValuePair* V8Context::BurstPersistent(PersistentValuePtr persistent,
                                         int* out_numKeys) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Persistent<v8::Value>* persist =
      static_cast<v8::Persistent<v8::Value>*>(persistent);

  mLastError.clear();

  v8::Local<v8::Value> maybeObject = persist->Get(mIsolate);

  if (!maybeObject->IsObject()) {
    return NULL;  // Triggers an error creation upstream.
  }

  // We can safely call `ToLocalChecked`, because
  // we've just created the local object above.
  v8::Local<v8::Object> object =
      maybeObject->ToObject(mContext.Get(mIsolate)).ToLocalChecked();
  v8::Local<v8::Array> keys(object->GetPropertyNames());
  int num_keys = keys->Length();
  *out_numKeys = num_keys;
  KeyValuePair* result = new KeyValuePair[num_keys];
  for (int i = 0; i < num_keys; i++) {
    v8::Local<v8::Value> key = keys->Get(i);
    result[i].keyName = strdup(str(key).c_str());
    result[i].value = new v8::Persistent<v8::Value>(mIsolate, object->Get(key));
  }

  return result;
}

std::string V8Context::report_exception(v8::TryCatch& try_catch) {
  std::stringstream ss;
  ss << "Uncaught exception: ";

  std::string exceptionStr = str(try_catch.Exception());
  if (exceptionStr == "[object Object]") {
    ss << to_json(mIsolate, try_catch.Exception());
  } else {
    ss << exceptionStr;
  }

  if (!try_catch.Message().IsEmpty()) {
    ss << std::endl
       << "at " << str(try_catch.Message()->GetScriptResourceName()) << ":"
       << try_catch.Message()->GetLineNumber() << ":"
       << try_catch.Message()->GetStartColumn() << ":"
       << str(try_catch.Message()->GetSourceLine());
  }

  if (!try_catch.StackTrace().IsEmpty()) {
    ss << std::endl << "Stack trace: " << str(try_catch.StackTrace());
  }

  return ss.str();
}

void V8Context::Throw(const char* errmsg) {
  v8::Locker locker(mIsolate);
  v8::Isolate::Scope isolate_scope(mIsolate);
  v8::HandleScope handle_scope(mIsolate);
  v8::Context::Scope context_scope(mContext.Get(mIsolate));
  v8::Local<v8::Value> err =
      v8::Exception::Error(v8::String::NewFromUtf8(mIsolate, errmsg));
  mIsolate->ThrowException(err);
}

char* V8Context::Error() {
  v8::Locker locker(mIsolate);
  return strdup(mLastError.c_str());
}

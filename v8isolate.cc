#include "v8isolate.h"

#include <cstdlib>
#include <cstring>

void* ArrayBufferAllocator::Allocate(size_t length) {
  void* data = AllocateUninitialized(length);
  return data == nullptr ? data : memset(data, 0, length);
}

void* ArrayBufferAllocator::AllocateUninitialized(size_t length) {
  return malloc(length);
}

void ArrayBufferAllocator::Free(void* data, size_t) { free(data); }

V8Isolate::V8Isolate() {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = &allocator;
  isolate_ = v8::Isolate::New(create_params);
}

void V8Isolate::PumpMessageLoop(v8::Platform *platform) {
  v8::Locker locker(isolate_);
  v8::Isolate::Scope isolate_scope(isolate_);
  v8::HandleScope handle_scope(isolate_);
  v8::SealHandleScope seal(isolate_);
  v8::platform::PumpMessageLoop(static_cast<v8::Platform *>(platform),isolate_);
}

V8Context* V8Isolate::MakeContext(uint32_t id) { return new V8Context(isolate_, id); }

V8Isolate::~V8Isolate() { isolate_->Dispose(); }

void V8Isolate::Terminate() { v8::V8::TerminateExecution(isolate_); }

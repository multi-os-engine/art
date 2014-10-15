/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_RUNTIME_ENTRYPOINTS_RUNTIME_ASM_ENTRYPOINTS_H_
#define ART_RUNTIME_ENTRYPOINTS_RUNTIME_ASM_ENTRYPOINTS_H_

namespace art {

#ifndef BUILDING_LIBART
#error "File and symbols only for use within libart."
#endif

// Entry point for deoptimization.
extern "C" void art_quick_deoptimize();
static inline uintptr_t GetQuickDeoptimizationEntryPoint() {
  return reinterpret_cast<uintptr_t>(art_quick_deoptimize);
}

// Return address of instrumentation stub.
extern "C" void art_quick_instrumentation_entry(void*);
static inline void* GetQuickInstrumentationEntryPoint() {
  return reinterpret_cast<void*>(art_quick_instrumentation_entry);
}

// The return_pc of instrumentation exit stub.
extern "C" void art_quick_instrumentation_exit();
static inline uintptr_t GetQuickInstrumentationExitPc() {
  return reinterpret_cast<uintptr_t>(art_quick_instrumentation_exit);
}

extern "C" void art_portable_to_interpreter_bridge(mirror::ArtMethod*);
static inline const void* GetPortableToInterpreterBridge() {
  return reinterpret_cast<void*>(art_portable_to_interpreter_bridge);
}

static inline const void* GetPortableToQuickBridge() {
  // TODO: portable to quick bridge. Bug: 8196384
  return GetPortableToInterpreterBridge();
}

extern "C" void art_quick_to_interpreter_bridge(mirror::ArtMethod*);
static inline const void* GetQuickToInterpreterBridge() {
  return reinterpret_cast<void*>(art_quick_to_interpreter_bridge);
}

static inline const void* GetQuickToPortableBridge() {
  // TODO: quick to portable bridge. Bug: 8196384
  return GetQuickToInterpreterBridge();
}

extern "C" void art_portable_proxy_invoke_handler();
static inline const void* GetPortableProxyInvokeHandler() {
  return reinterpret_cast<void*>(art_portable_proxy_invoke_handler);
}

extern "C" void art_quick_proxy_invoke_handler();
static inline const void* GetQuickProxyInvokeHandler() {
  return reinterpret_cast<void*>(art_quick_proxy_invoke_handler);
}

extern "C" void* art_jni_dlsym_lookup_stub(JNIEnv*, jobject);
static inline void* GetJniDlsymLookupStub() {
  return reinterpret_cast<void*>(art_jni_dlsym_lookup_stub);
}

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_RUNTIME_ASM_ENTRYPOINTS_H_

/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "art_method.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "ScopedUtfChars.h"
#include "oat_file_assistant.h"
#include "oat_quick_method_header.h"
#include "scoped_thread_state_change.h"

namespace art {

static constexpr int kStateUnknown = 0;       // == Main.STATE_UNKNOWN
static constexpr int kStateInterpreter = 1;   // == Main.STATE_INTERPRETER
static constexpr int kStateJit = 2;           // == Main.STATE_JIT
static constexpr int kStateAot = 3;           // == Main.STATE_AOT

class CompilationStateVisitor : public StackVisitor {
 public:
  CompilationStateVisitor(Thread* thread, const char* fn)
      SHARED_REQUIRES(Locks::mutator_lock_)
      : StackVisitor(thread, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
        fn_(fn),
        state_(kStateUnknown) {}

  bool VisitFrame() SHARED_REQUIRES(Locks::mutator_lock_) {
    ArtMethod* m = GetMethod();
    std::string m_name(m->GetName());

    if (m_name.compare(fn_) == 0) {
      jit::Jit* jit = Runtime::Current()->GetJit();
      if (jit != nullptr &&
          jit->GetCodeCache()->ContainsPc(
              reinterpret_cast<const void*>(GetCurrentQuickFramePc()))) {
        state_ = kStateJit;
      } else if (IsCurrentFrameInInterpreter()) {
        state_ = kStateInterpreter;
      } else {
        state_ = kStateAot;
      }
      return false;
    }
    return true;
  }

  const char* const fn_;
  int state_;
};

extern "C" JNIEXPORT jint JNICALL Java_Main_nativeGetCompilationState(JNIEnv* env,
                                                                      jclass,
                                                                      jstring fn) {
  ScopedUtfChars chars(env, fn);
  if (chars.c_str() == nullptr) {
    return 0;
  }
  ScopedObjectAccess soa(Thread::Current());
  CompilationStateVisitor visitor(soa.Self(), chars.c_str());
  visitor.WalkStack();
  return visitor.state_;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_nativeHasJit(JNIEnv*, jclass) {
  return Runtime::Current()->GetJit() != nullptr;
}

extern "C" JNIEXPORT void JNICALL Java_Main_transitionToAotCode(JNIEnv*, jclass klass) {
  const DexFile* dex_file;
  {
    ScopedObjectAccess soa(Thread::Current());
    dex_file = soa.Decode<mirror::Class*>(klass)->GetDexCache()->GetDexFile();
  }
  std::string location = dex_file->GetLocation();
  {
    OatFileAssistant assistant(location.c_str(),
                               kRuntimeISA,
                               /* profile_changed */ false,
                               /* load_executable */ true);
    std::string error_msg;
    OatFileAssistant::ResultOfAttemptToUpdate result =
        assistant.GenerateOatFile(CompilerFilter::kEverything, &error_msg);
    CHECK(result == OatFileAssistant::kUpdateSucceeded);
  }

  Runtime::Current()->ReplaceOatFileForDexFile(location.c_str());
}

}  // namespace art

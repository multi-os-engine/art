/*
 * Copyright (C) 2014 The Android Open Source Project
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

#if defined(__arm__)
#include "interpreter_common.h"
#include "arch/arm/xlator/translator.h"

namespace art {
namespace interpreter {

struct TxMethod {
  uint32_t count;
  mirror::EntryPointFromInterpreter* txmethod;
};

static bool kDebugPrints = false;
// helper subroutines
extern "C" {
extern uint32_t art_memcpyHelper[];
extern uint32_t art_CheckSuspendHelper[];
extern uint32_t art_ResolveStringHelper[];
extern uint32_t art_HandlePendingExceptionHelper[];
extern uint32_t art_ResolveVerifyAndClinitHelper[];
extern uint32_t art_MonitorEnterHelper[];
extern uint32_t art_MonitorExitHelper[];
extern uint32_t art_CheckArrayAssignHelper[];
extern uint32_t art_InstanceOfHelper[];
extern uint32_t art_ThrowDivideByZeroExceptionHelper[];
extern uint32_t art_DoIntDivideHelper[];
extern uint32_t art_DoIntRemainderHelper[];
extern uint32_t art_ThrowNullPointerExceptionHelper[];
extern uint32_t art_AllocObjectFromCodeHelper[];
extern uint32_t art_AllocArrayFromCodeHelper[];
extern uint32_t art_DoFilledNewArrayHelper[];
extern uint32_t art_DoFilledNewArrayRangeHelper[];
extern uint32_t art_FillArrayDataHelper[];
extern uint32_t art_ThrowHelper[];
extern uint32_t art_fmodfHelper[];
extern uint32_t art_fmodHelper[];
extern uint32_t art_ThrowArrayIndexOutOfBoundsExceptionHelper[];
extern uint32_t art_DoLongDivideHelper[];
extern uint32_t art_DoLongRemainderHelper[];
extern uint32_t art_ThrowStackOverflowHelper[];
extern uint32_t art_ThrowNullPointerExceptionForFieldAccessHelper[];
extern uint32_t art_ResolveVirtualMethodHelper[];
extern uint32_t art_DoCallHelper[];
extern uint32_t art_DoCallRangeHelper[];
extern uint32_t art_PrintHelper[];
extern uint32_t art_BreakpointHelper[];
extern uint32_t art_RegDumpHelper[];
extern uint32_t art_HexDumpHelper[];
extern uint32_t art_WriteBarrierFieldHelper[];
extern uint32_t art_ResolveMethodQuickHelper[];
extern uint32_t art_SetExceptionHelper[];
extern uint32_t art_ResolveVerifyAndClinitHelper[];
extern uint32_t art_ThrowClassCastExceptionHelper[];
extern uint32_t art_LongToFloatHelper[];
extern uint32_t art_LongToDoubleHelper[];
extern uint32_t art_FloatToLongHelper[];
extern uint32_t art_FloatToDoubleHelper[];
extern uint32_t art_DoubleToLongHelper[];
extern uint32_t art_DoubleToFloatHelper[];
extern uint32_t art_ThrowAbstractMethodErrorHelper[];
extern uint32_t art_FloatToIntHelper[];
extern uint32_t art_DoubleToIntHelper[];
extern uint32_t art_CompareFloatLessHelper[];
extern uint32_t art_CompareFloatGreaterHelper[];
extern uint32_t art_CompareDoubleLessHelper[];
extern uint32_t art_CompareDoubleGreaterHelper[];
extern uint32_t art_ResolveDirectMethodHelper[];
extern uint32_t art_ResolveSuperMethodHelper[];
extern uint32_t art_ResolveInterfaceMethodHelper[];
extern uint32_t art_ResolveStaticMethodHelper[];
extern uint32_t art_ResolveFieldHelper_InstanceObjectRead[];
extern uint32_t art_ResolveFieldHelper_InstanceObjectWrite[];
extern uint32_t art_ResolveFieldHelper_InstancePrimitiveRead[];
extern uint32_t art_ResolveFieldHelper_InstancePrimitiveWrite[];
extern uint32_t art_ResolveFieldHelper_StaticObjectRead[];
extern uint32_t art_ResolveFieldHelper_StaticObjectWrite[];
extern uint32_t art_ResolveFieldHelper_StaticPrimitiveRead[];
extern uint32_t art_ResolveFieldHelper_StaticPrimitiveWrite[];
extern uint32_t art_PushShadowFrameHelper[];
extern uint32_t art_PopShadowFrameHelper[];

uint32_t* art_xlator_helpers[] = {
     art_memcpyHelper,
     art_CheckSuspendHelper,
     art_ResolveStringHelper,
     art_HandlePendingExceptionHelper,
     art_ResolveVerifyAndClinitHelper,
     art_MonitorEnterHelper,
     art_MonitorExitHelper,
     art_CheckArrayAssignHelper,
     art_InstanceOfHelper,
     art_ThrowDivideByZeroExceptionHelper,
     art_DoIntDivideHelper,
     art_DoIntRemainderHelper,
     art_ThrowNullPointerExceptionHelper,
     art_AllocObjectFromCodeHelper,
     art_AllocArrayFromCodeHelper,
     art_DoFilledNewArrayHelper,
     art_DoFilledNewArrayRangeHelper,
     art_FillArrayDataHelper,
     art_ThrowHelper,
     art_fmodfHelper,
     art_fmodHelper,
     art_ThrowArrayIndexOutOfBoundsExceptionHelper,
     art_DoLongDivideHelper,
     art_DoLongRemainderHelper,
     art_ThrowStackOverflowHelper,
     art_ThrowNullPointerExceptionForFieldAccessHelper,
     art_ResolveVirtualMethodHelper,
     art_DoCallHelper,
     art_DoCallRangeHelper,
     art_PrintHelper,
     art_BreakpointHelper,
     art_RegDumpHelper,
     art_HexDumpHelper,
     art_WriteBarrierFieldHelper,
     art_ResolveMethodQuickHelper,
     art_SetExceptionHelper,
     art_ThrowClassCastExceptionHelper,
     art_LongToFloatHelper,
     art_LongToDoubleHelper,
     art_FloatToLongHelper,
     art_FloatToDoubleHelper,
     art_DoubleToLongHelper,
     art_DoubleToFloatHelper,
     art_ThrowAbstractMethodErrorHelper,
     art_FloatToIntHelper,
     art_DoubleToIntHelper,
     art_CompareFloatLessHelper,
     art_CompareFloatGreaterHelper,
     art_CompareDoubleLessHelper,
     art_CompareDoubleGreaterHelper,
     art_ResolveDirectMethodHelper,
     art_ResolveSuperMethodHelper,
     art_ResolveInterfaceMethodHelper,
     art_ResolveStaticMethodHelper,
     art_ResolveFieldHelper_InstanceObjectRead,
     art_ResolveFieldHelper_InstanceObjectWrite,
     art_ResolveFieldHelper_InstancePrimitiveRead,
     art_ResolveFieldHelper_InstancePrimitiveWrite,
     art_ResolveFieldHelper_StaticObjectRead,
     art_ResolveFieldHelper_StaticObjectWrite,
     art_ResolveFieldHelper_StaticPrimitiveRead,
     art_ResolveFieldHelper_StaticPrimitiveWrite,
     art_PushShadowFrameHelper,
     art_PopShadowFrameHelper,
};



extern void davebreak(int);
}     // extern "C"

typedef std::map<uint32_t, TxMethod> MethodCounts;

arm::ChunkTable chunk_table;
arm::ARMTranslator translator(chunk_table, sizeof(art_xlator_helpers));
MethodCounts method_counts;
Mutex translator_lock("Main Translator");
uint64_t total_translation_time;
int64_t last_report_time;
constexpr int64_t kReportIntervalSecs = 1;

// This function is a launch point for invoking the fallback interpreter from a method
// whose translation failed for whatever reason.  It is used so that we never try
// to translate the method again.
void UntranslatabledMethod(Thread* self, MethodHelper& mh, const DexFile::CodeItem* code_item,
                                ShadowFrame* shadow_frame, JValue* result_register) {
  JValue result;
#ifdef __clang__
  result = ExecuteSwitchImpl<false, false>(self, mh, code_item, *shadow_frame, *result_register);
#else
  result = ExecuteGotoImpl<false, false>(self, mh, code_item, *shadow_frame, *result_register);
#endif
  *result_register = result;
}


void ResetTranslator() {
  MutexLock mu(Thread::Current(), translator_lock);
  method_counts.clear();
  translator.Clear();
  total_translation_time = 0;
}

// Translate a method into a chunk program.  This will be called when there is no
// translation available and will store the translated method into the ArtMethod's
// EntryPointFromInterpreter pointer.  If the translation fails we store the
// UntranslatabledMethod function into the same entry point so that we never try
// to translate the same method twice.
JValue ExecuteTranslatorImpl(Thread* self, MethodHelper& mh, const DexFile::CodeItem* code_item,
                                ShadowFrame& shadow_frame, JValue result_register) {
  self->VerifyStack();
  mirror::ArtMethod* method = const_cast<mirror::ArtMethod*>(mh.GetMethod());
  uint32_t method_as_int = reinterpret_cast<uint32_t>(method) >> 2;

  const uint32_t kTranslationCallThreshold = 1;     // TODO: increase this to 2 when everything works
  bool bail = true;
  MethodCounts::iterator mi;
  uint64_t start_time = NanoTime();
  bool first_time = false;
  bool report = false;

  // We want to translate only after a threshold has been reached.
  {
    MutexLock mu(Thread::Current(), translator_lock);
    mi = method_counts.find(method_as_int);
    if (mi != method_counts.end()) {
      TxMethod &m = mi->second;
      uint32_t count = ++m.count;        // Increment method count
      if (kDebugPrints) {
        LOG(INFO) << "Method has been called " << count << " times: " << PrettyMethod(method, true);
      }
      bail = count < kTranslationCallThreshold;
    } else {
      first_time = true;
      TxMethod m;
      m.count = 1;                // One call so far.
      m.txmethod = nullptr;       // And no translated method yet.
      method_counts[method_as_int] = m;
      bail = false;         // TODO: remove this when things work
      mi = method_counts.find(method_as_int);
    }

    uint64_t now = MilliTime();
    int64_t diff_ms = now - last_report_time;
    if ((diff_ms / 1000) > kReportIntervalSecs) {
      report = true;
    }
    last_report_time = now;
  }

  if (bail) {
    // Don't want to translate this yet.
    // LOG(INFO) << "Bailing to portable interpreter";
#ifdef __clang__
    return ExecuteSwitchImpl<false, false>(self, mh, code_item, shadow_frame, result_register);
#else
    return ExecuteGotoImpl<false, false>(self, mh, code_item, shadow_frame, result_register);
#endif
  } else {
    // It is possible that we get here from a path other than from the interpreter.  In this
    // case we don't want to translate the method more than once.  So we store the txmethod
    // pointer in the methods_count map and call it directly.
    if (!first_time && mi->second.txmethod != nullptr) {
      if (kDebugPrints) {
        LOG(INFO) << "Method " << PrettyMethod(method, true) <<
            " has already been translated, calling it";
      }
      // Already translated this method?  If so, call it
      // But first we need to pop the top shadow frame since the translated method will
      // push it back.
      self->PopShadowFrame();
      JValue result;
      mi->second.txmethod(self, mh, code_item, &shadow_frame, &result);
      self->PushShadowFrame(&shadow_frame);
      return result;
    }
  }

  // Ensure static methods are initialized.
  if (method->IsStatic()) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    StackHandleScope<1> hs(self);
    Handle<mirror::Class> h_class(hs.NewHandle(method->GetDeclaringClass()));
    if (UNLIKELY(!class_linker->EnsureInitialized(h_class, true, true))) {
      CHECK(self->IsExceptionPending());
      // Exception during initialization.
      return JValue();
    }
  }

  const uint16_t* code = code_item->insns_;
  const uint16_t* endcode = code + code_item->insns_size_in_code_units_;
  if (UNLIKELY(code == endcode)) {
    // Empty method.
    return JValue();
  }

  if (true || kDebugPrints) {
    LOG(INFO) << "Translating method " << PrettyMethod(method, true) << " (" << method << ")";
  }


  // Try to translate the method.  This might fail and if so, will return a nullptr.
  mirror::EntryPointFromInterpreter* txmethod = translator.Translate(method, code, endcode);

  if (UNLIKELY(txmethod == nullptr)) {
    // Translation failed, need to bail and make sure this method uses another
    // interpreter.

    LOG(INFO) << "Translation failed";
    // Set the entry point so that we don't try to translate this again.
    method->SetEntryPointFromInterpreter(UntranslatabledMethod);

    // Now invoke the method using the fallback interpreter.
#ifdef __clang__
    return ExecuteSwitchImpl<false, false>(self, mh, code_item, shadow_frame, result_register);
#else
    return ExecuteGotoImpl<false, false>(self, mh, code_item, shadow_frame, result_register);
#endif
  }

  {
    MutexLock mu(Thread::Current(), translator_lock);
    // store translated method into the ArtMethod so that next time it will bypass the
    // translation and go directly to the generated code.
    method->SetEntryPointFromInterpreter(txmethod);

    // Record the translated method pointer in the methods map so that we don't
    // translate it twice.
    mi->second.txmethod = txmethod;
    uint64_t end_time = NanoTime();

    uint64_t elapsed = end_time - start_time;
    total_translation_time += elapsed;
    if (report) {
      LOG(INFO) << "Total time in translation: " << PrettyDuration(total_translation_time);
      translator.ShowCacheSize();
    }
  }

  // We've just got a new translation, let's call it.
  // But first we need to pop the top shadow frame since the translated method will
  // push it back.
  self->PopShadowFrame();

  JValue result;
  txmethod(self, mh, code_item, &shadow_frame, &result);
  self->PushShadowFrame(&shadow_frame);
  return result;
}


}  // namespace interpreter
}  // namespace art

#endif    // defined(__arm__)

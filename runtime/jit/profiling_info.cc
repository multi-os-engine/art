/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "art_method-inl.h"
#include "dex_instruction.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "profiling_info.h"
#include "scoped_thread_state_change.h"
#include "thread.h"

namespace art {

ProfilingInfo* ProfilingInfo::Create(ArtMethod* method) {
  DexPcToCache map;

  const uint16_t* code_ptr = nullptr; 
  const uint16_t* code_end = nullptr; 
  {
    ScopedObjectAccess soa(Thread::Current());
    DCHECK(!method->IsNative());
    const DexFile::CodeItem& code_item = *method->GetCodeItem();
    code_ptr = code_item.insns_;
    code_end = code_item.insns_ + code_item.insns_size_in_code_units_;
  }

  uint32_t dex_pc = 0;
  uint16_t current_cache_index = 0;
  while (code_ptr < code_end) {
    const Instruction& instruction = *Instruction::At(code_ptr);
    switch (instruction.Opcode()) {
      case Instruction::INVOKE_VIRTUAL:
      case Instruction::INVOKE_VIRTUAL_RANGE:
      case Instruction::INVOKE_VIRTUAL_QUICK:
      case Instruction::INVOKE_VIRTUAL_RANGE_QUICK:
      case Instruction::INVOKE_INTERFACE:
      case Instruction::INVOKE_INTERFACE_RANGE:
        map.Insert(std::make_pair(dex_pc, current_cache_index));
        current_cache_index += kIndividualCacheSize;
        break;

      default:
        break;
    }
    dex_pc += instruction.SizeInCodeUnits();
    code_ptr += instruction.SizeInCodeUnits();
  }

  jit::JitCodeCache* code_cache = Runtime::Current()->GetJit()->GetCodeCache();
  size_t profile_info_size = sizeof(ProfilingInfo) + (kIndividualCacheSize * map.Size());
  size_t total_size =
      profile_info_size + (sizeof(std::pair<uint32_t, uint32_t>) * map.NumBuckets());
  uint8_t* data = code_cache->ReserveData(Thread::Current(), total_size);

  return new (data) ProfilingInfo(
      map, reinterpret_cast<std::pair<uint32_t, uint32_t>*>(data + profile_info_size));
}

void ProfilingInfo::AddInvokeInfo(Thread* self, uint32_t dex_pc, mirror::Class* cls) {
  uint32_t entry_in_cache = dex_pc_to_cache_entry_.Find(dex_pc)->second;
  ScopedObjectAccess soa(self);
  for (size_t i = entry_in_cache; i < kIndividualCacheSize; ++i) {
    mirror::Class* existing = cache_[i].Read<kWithoutReadBarrier>();
    if (existing == cls) {
      return;
    } else if (existing == nullptr) {
      GcRoot<mirror::Class> expected_root(nullptr);
      GcRoot<mirror::Class> desired_root(cls);
      if (!reinterpret_cast<Atomic<GcRoot<mirror::Class>>*>(&cache_[i])->
              CompareExchangeStrongSequentiallyConsistent(expected_root, desired_root)) {
        // Retry;
        --i;
      } else {
        // Success, just return.
        return;
      }
    }
  }
  // Unsuccessfull - cache is full, making it megamorphic.
}

}  // namespace art

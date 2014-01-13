/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include <algorithm>
#include <utility>
#include "leb128_encoder.h"
#include "thread.h"
#include "thread-inl.h"
#include "base/mutex.h"
#include "base/mutex-inl.h"
#include "base/logging.h"
#include "driver/compiler_driver.h"
#include "UniquePtr.h"

#include "dex_file_to_method_inliner_map.h"

namespace art {

DexFileToMethodInlinerMap::DexFileToMethodInlinerMap()
    : lock_("DexFileToMethodInlinerMap lock", kDexFileToMethodInlinerMapLock) {
}

DexFileToMethodInlinerMap::~DexFileToMethodInlinerMap() {
  for (auto& entry : inliners_) {
    delete entry.second;
  }
}

DexFileMethodInliner* DexFileToMethodInlinerMap::GetMethodInliner(const DexFile* dex_file) {
  Thread* self = Thread::Current();
  {
    ReaderMutexLock mu(self, lock_);
    auto it = inliners_.find(dex_file);
    if (it != inliners_.end()) {
      return it->second;
    }
  }

  // We need to acquire our lock_ to modify inliners_ but we want to release it
  // before we initialize the new inliner. However, we need to acquire the
  // new inliner's lock_ before we release our lock_ to prevent another thread
  // from using the uninitialized inliner. This requires explicit calls to
  // ExclusiveLock()/ExclusiveUnlock() on one of the locks, the other one
  // can use WriterMutexLock.
  DexFileMethodInliner* locked_inliner;
  {
    WriterMutexLock mu(self, lock_);
    DexFileMethodInliner** inliner = &inliners_[dex_file];  // inserts new entry if not found
    if (*inliner) {
      return *inliner;
    }
    *inliner = new DexFileMethodInliner;
    DCHECK(*inliner != nullptr);
    locked_inliner = *inliner;
    locked_inliner->lock_.ExclusiveLock(self);  // Acquire inliner's lock_ before releasing lock_.
  }
  locked_inliner->FindIntrinsics(dex_file);
  locked_inliner->lock_.ExclusiveUnlock(self);
  return locked_inliner;
}

const std::vector<uint8_t>* DexFileToMethodInlinerMap::CreateInlineRefs(
    const std::vector<const DexFile*>& dex_files) {
  CHECK_LE(dex_files.size(), 0xffffu);  // We've got only 16 bits for dex file index.
  std::vector<DexFileMethodInliner::InlinedMethodEntry> inlined_methods;
  inlined_methods.reserve(128);

  ReaderMutexLock mu(Thread::Current(), lock_);
  Leb128EncodingVector reference_data;
  reference_data.Reserve(1024);
  for (const auto& dex_to_inliner : inliners_) {
    dex_to_inliner.second->WriteInlinedMethodRefs(&inlined_methods, &reference_data, dex_files);
  }
  if (inlined_methods.empty()) {
    return nullptr;
  }

  size_t header_size = sizeof(uint32_t) + 8u * inlined_methods.size();
  UniquePtr<std::vector<uint8_t> > result(new std::vector<uint8_t>());
  result->reserve(header_size + reference_data.GetData().size());
  for (const DexFileMethodInliner::InlinedMethodEntry& entry : inlined_methods) {
    result->push_back(entry.dex_file_index & 0xffu);
    result->push_back((entry.dex_file_index >> 8) & 0xffu);
    result->push_back(entry.method_index & 0xffu);
    result->push_back((entry.method_index >> 8) & 0xffu);
    // Adjust offset by the header size
    size_t offset =  header_size + entry.refs_offset;
    result->push_back(offset & 0xffu);
    result->push_back((offset >> 8) & 0xffu);
    result->push_back((offset >> 16) & 0xffu);
    result->push_back((offset >> 24) & 0xffu);
  }
  result->insert(result->end(), reference_data.GetData().begin(), reference_data.GetData().end());
  return result.release();
}

}  // namespace art

/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_OBJECT_UTILS_INL_H_
#define ART_RUNTIME_OBJECT_UTILS_INL_H_

#include "object_utils.h"

#include "sirt_ref-inl.h"

namespace art {

template<class T>
ObjectLock<T>::ObjectLock(Thread* self, const SirtRef<T>* object)
    : self_(self), obj_(object) {
  CHECK(object != nullptr);
  CHECK(object->get() != nullptr);
  obj_->get()->MonitorEnter(self_);
}

template<class T>
ObjectLock<T>::~ObjectLock() {
  obj_->get()->MonitorExit(self_);
}

template<class T>
void ObjectLock<T>::WaitIgnoringInterrupts() {
  Monitor::Wait(self_, obj_->get(), 0, 0, false, kWaiting);
}

template<class T>
void ObjectLock<T>::Notify() {
  obj_->get()->Notify(self_);
}

template<class T>
void ObjectLock<T>::NotifyAll() {
  obj_->get()->NotifyAll(self_);
}

mirror::String* MethodHelper::GetNameAsString() {
  const DexFile& dex_file = GetDexFile();
  uint32_t dex_method_idx = method_->GetDexMethodIndex();
  const DexFile::MethodId& method_id = dex_file.GetMethodId(dex_method_idx);
  SirtRef<mirror::DexCache> dex_cache(Thread::Current(), GetDexCache());
  return GetClassLinker()->ResolveString(dex_file, method_id.name_idx_, dex_cache);
}

mirror::String* MethodHelper::ResolveString(uint32_t string_idx) {
  mirror::String* s = method_->GetDexCacheStrings()->Get(string_idx);
  if (UNLIKELY(s == nullptr)) {
    SirtRef<mirror::DexCache> dex_cache(Thread::Current(), GetDexCache());
    s = GetClassLinker()->ResolveString(GetDexFile(), string_idx, dex_cache);
  }
  return s;
}

}  // namespace art

#endif  // ART_RUNTIME_OBJECT_UTILS_INL_H_

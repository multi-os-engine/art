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

#include "mir_annotations.h"

#include <string.h>

#include "base/logging.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_driver-inl.h"
#include "mirror/class_loader.h"  // Only to allow casts in SirtRef<ClassLoader>.
#include "mirror/dex_cache.h"     // Only to allow casts in SirtRef<DexCache>.
#include "scoped_thread_state_change.h"
#include "sirt_ref.h"

namespace art {

void IFieldAnnotation::Resolve(CompilerDriver* compiler_driver, const DexCompilationUnit* mUnit,
                               IFieldAnnotation* annotations, size_t count) {
  if (kIsDebugBuild) {
    DCHECK(annotations != nullptr);
    DCHECK_NE(count, 0u);
    for (auto it = annotations, end = annotations + count; it != end; ++it) {
      IFieldAnnotation unresolved(it->field_idx_);
      DCHECK_EQ(memcmp(&unresolved, &*it, sizeof(*it)), 0);
    }
  }

  // We're going to resolve fields and check access in a tight loop. It's better to hold
  // the lock and needed references once than re-acquiring them again and again.
  ScopedObjectAccess soa(Thread::Current());
  SirtRef<mirror::DexCache> dex_cache(soa.Self(), compiler_driver->GetDexCache(mUnit));
  SirtRef<mirror::ClassLoader> class_loader(soa.Self(),
      compiler_driver->GetClassLoader(soa, mUnit));
  SirtRef<mirror::Class> referrer_class(soa.Self(),
      compiler_driver->ResolveCompilingMethodsClass(soa, dex_cache, class_loader, mUnit));
  // Even if the referrer class is unresolved (i.e. we're compiling a method without class
  // definition) we still want to resolve fields and record all available info.

  for (auto it = annotations, end = annotations + count; it != end; ++it) {
    uint32_t field_idx = it->field_idx_;
    mirror::ArtField* resolved_field =
        compiler_driver->ResolveField(soa, dex_cache, class_loader, mUnit, field_idx, false);
    if (UNLIKELY(resolved_field == nullptr)) {
      continue;
    }
    compiler_driver->GetResolvedFieldDexFileLocation(resolved_field,
        &it->declaring_dex_file_, &it->declaring_class_idx_, &it->declaring_field_idx_);
    it->is_volatile_ = compiler_driver->IsFieldVolatile(resolved_field) ? 1u : 0u;

    std::pair<bool, bool> fast_path = compiler_driver->IsFastInstanceField(
        dex_cache.get(), referrer_class.get(), resolved_field, field_idx, &it->field_offset_);
    it->fast_get_ = fast_path.first ? 1u : 0u;
    it->fast_put_ = fast_path.second ? 1u : 0u;
  }
}

void SFieldAnnotation::Resolve(CompilerDriver* compiler_driver, const DexCompilationUnit* mUnit,
                               SFieldAnnotation* annotations, size_t count) {
  if (kIsDebugBuild) {
    DCHECK(annotations != nullptr);
    DCHECK_NE(count, 0u);
    for (auto it = annotations, end = annotations + count; it != end; ++it) {
      SFieldAnnotation unresolved(it->field_idx_);
      DCHECK_EQ(memcmp(&unresolved, &*it, sizeof(*it)), 0);
    }
  }

  // We're going to resolve fields and check access in a tight loop. It's better to hold
  // the lock and needed references once than re-acquiring them again and again.
  ScopedObjectAccess soa(Thread::Current());
  SirtRef<mirror::DexCache> dex_cache(soa.Self(), compiler_driver->GetDexCache(mUnit));
  SirtRef<mirror::ClassLoader> class_loader(soa.Self(),
      compiler_driver->GetClassLoader(soa, mUnit));
  SirtRef<mirror::Class> referrer_class(soa.Self(),
      compiler_driver->ResolveCompilingMethodsClass(soa, dex_cache, class_loader, mUnit));
  // Even if the referrer class is unresolved (i.e. we're compiling a method without class
  // definition) we still want to resolve fields and record all available info.

  for (auto it = annotations, end = annotations + count; it != end; ++it) {
    uint32_t field_idx = it->field_idx_;
    mirror::ArtField* resolved_field =
        compiler_driver->ResolveField(soa, dex_cache, class_loader, mUnit, field_idx, true);
    if (UNLIKELY(resolved_field == nullptr)) {
      continue;
    }
    compiler_driver->GetResolvedFieldDexFileLocation(resolved_field,
        &it->declaring_dex_file_, &it->declaring_class_idx_, &it->declaring_field_idx_);
    it->is_volatile_ = compiler_driver->IsFieldVolatile(resolved_field) ? 1u : 0u;

    bool is_referrers_class, is_initialized;
    std::pair<bool, bool> fast_path = compiler_driver->IsFastStaticField(
        dex_cache.get(), referrer_class.get(), resolved_field, field_idx, &it->field_offset_,
        &it->storage_index_, &is_referrers_class, &is_initialized);
    it->fast_get_ = fast_path.first ? 1u : 0u;
    it->fast_put_ = fast_path.second ? 1u : 0u;
    it->is_referrers_class_ = is_referrers_class ? 1u : 0u;
    it->is_initialized_ = is_initialized ? 1u : 0u;
  }
}

}  // namespace art

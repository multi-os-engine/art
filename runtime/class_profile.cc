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

#include "class_profile.h"

#include "class_linker.h"
#include "mirror/dex_cache-inl.h"

namespace art {

void ClassProfile::Collect() {
  ScopedObjectAccess soa(Thread::Current());
  // Loop through all the dex caches.
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  Thread* const self = Thread::Current();
  uint64_t start_time = NanoTime();
  ReaderMutexLock mu(self, *class_linker->DexLock());
  for (const ClassLinker::DexCacheData& data : class_linker->GetDexCachesData()) {
    mirror::DexCache* dex_cache = down_cast<mirror::DexCache*>(self->DecodeJObject(data.weak_root));
    if (dex_cache == nullptr) {
      continue;
    }
    const DexFile* dex_file = dex_cache->GetDexFile();
    const std::string& location = dex_file->GetLocation();
    const size_t num_class_defs = dex_file->NumClassDefs();
    DexFileProfileData* profile_data;
    auto found = profiles_.find(location);
    if (found == profiles_.end()) {
      profile_data = &profiles_[location];
      size_t num_bytes = RoundUp(num_class_defs, kBitsPerByte) / kBitsPerByte;
      profile_data->num_class_defs = num_class_defs;
      profile_data->resolved_bitmap.reset(new uint8_t[num_bytes]);
      profile_data->verified_bitmap.reset(new uint8_t[num_bytes]);
    } else {
      profile_data = &found->second;
      CHECK_EQ(profile_data->num_class_defs, num_class_defs);
    }
    // Use the resolved types, this will miss array classes.
    const size_t num_types = dex_file->NumTypeIds();
    VLOG(class_linker) << "Collecting class profile for dex file " << location
                       << " types=" << num_types << " class_defs=" << num_class_defs;
    for (size_t i = 0; i < num_types; ++i) {
      mirror::Class* klass = dex_cache->GetResolvedType(i);
      if (klass == nullptr) {
        continue;
      }
      DCHECK(!klass->IsProxyClass());
      mirror::DexCache* klass_dex_cache = klass->GetDexCache();
      if (klass_dex_cache == dex_cache) {
        const size_t class_def_idx = klass->GetDexClassDefIndex();
        DCHECK(klass->IsResolved());
        CHECK_LT(class_def_idx, num_class_defs);
        SetInBitmap(class_def_idx, &profile_data->resolved_bitmap[0]);
        if (klass->IsVerified()) {
          SetInBitmap(class_def_idx, &profile_data->verified_bitmap[0]);
        }
      }
    }
  }
  VLOG(class_linker) << "Colleting profile took " << PrettyDuration(NanoTime() - start_time);
}

void ClassProfile::Dump(std::ostream& os) {
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  Thread* const self = Thread::Current();
  std::unordered_map<std::string, const DexFile*> location_to_dex_file;
  {
    ScopedObjectAccess soa(self);
    ReaderMutexLock mu(self, *class_linker->DexLock());
    for (const ClassLinker::DexCacheData& data : class_linker->GetDexCachesData()) {
      mirror::DexCache* dex_cache = down_cast<mirror::DexCache*>(
          self->DecodeJObject(data.weak_root));
      if (dex_cache != nullptr) {
        const DexFile* dex_file = dex_cache->GetDexFile();
        location_to_dex_file.emplace(dex_file->GetLocation(), dex_file);
      }
    }
  }
  for (const auto& pair : profiles_) {
    const std::string& dex_file_name = pair.first;
    os << "Dex file " << dex_file_name << "\n";
    const DexFile* dex_file = nullptr;
    auto found = location_to_dex_file.find(dex_file_name);
    if (found != location_to_dex_file.end()) {
      dex_file = found->second;
    } else {
      // Try to open the dex file from a file.
      std::vector<std::unique_ptr<const DexFile>> opened_dex_files;
      std::string error_msg;
      if (!DexFile::Open(dex_file_name.c_str(),
                         dex_file_name.c_str(),
                         &error_msg,
                         &opened_dex_files)) {
        os << "Failed to open dex file " << dex_file_name << " with error " << error_msg << "\n";
      } else if (opened_dex_files.size() != 1) {
        os << "Multiple dex files in " << dex_file_name << "\n";
      } else {
        dex_file = opened_dex_files[0].get();
      }
    }

    const DexFileProfileData& data = pair.second;
    size_t resolved = 0, verified = 0;
    for (size_t i = 0; i < data.num_class_defs; ++i) {
      if (TestBitmap(i, &data.resolved_bitmap[0])) {
        ++resolved;
        os << "Class " << i << ": ";
        if (TestBitmap(i, &data.verified_bitmap[0])) {
          ++verified;
          os << "verified ";
        } else {
          os << "resolved ";
        }
        if (dex_file != nullptr) {
          const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
          const DexFile::TypeId& type_id = dex_file->GetTypeId(class_def.class_idx_);
          const char* descriptor = dex_file->GetTypeDescriptor(type_id);
          os << descriptor;
        } else {
          os << "unknown";
        }
        os << "\n";
      } else {
        DCHECK(!TestBitmap(i, &data.verified_bitmap[0]));
      }
    }
    os << "Resolved=" << resolved << " verified=" << verified << "\n";
  }
}

size_t ClassProfile::Serialize(std::vector<uint8_t>* out) {
  out->push_back(0u);
  return 0;
}

size_t ClassProfile::Deserialize(const uint8_t* in ATTRIBUTE_UNUSED, size_t in_size ATTRIBUTE_UNUSED) {
  return 0;
}

void ClassProfile::Write(const char* file_name ATTRIBUTE_UNUSED) {
  /*
  std::unique_ptr<File> oat_file(OS::CreateEmptyFileWriteOnly(file_name));
  for (auto& pair : profiles_) {
    const std::string& name = pair.first;
    oat_file->Write()
    // std::string, DexFileProfileData
  }*/
}

void ClassProfile::Read() {
}

}  // namespace art

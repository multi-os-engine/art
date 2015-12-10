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

#include "class_profile.h"

#include "class_linker.h"
#include "mirror/dex_cache-inl.h"

namespace art {

static inline size_t BitmapBytes(size_t num_class_defs) {
  return RoundUp(num_class_defs, kBitsPerByte) / kBitsPerByte;
}

template <typename T>
static bool Read(const uint8_t** in, size_t* remain_size, T& out) {
  if (*remain_size < sizeof(T)) {
    return false;
  }
  out = *reinterpret_cast<const T*>(*in);
  *in += sizeof(T);
  *remain_size -= sizeof(T);
  return true;
}

template <typename T>
static void Write(std::vector<uint8_t>* out, const T& data) {
  out->insert(out->end(),
              reinterpret_cast<const uint8_t*>(&data),
              reinterpret_cast<const uint8_t*>(&data) + sizeof(data));
}

DexCacheProfileData::DexCacheProfileData(const DexFile& dex_file)
    : DexCacheProfileData(dex_file.GetLocationChecksum(), dex_file.NumClassDefs()) {}

DexCacheProfileData::DexCacheProfileData(uint32_t checksum, uint32_t num_class_defs)
    : checksum_(checksum),
      num_class_defs_(num_class_defs),
      resolved_bitmap_(new uint8_t[BitmapBytes(num_class_defs)]) {}

void DexCacheProfileData::Update(mirror::DexCache* dex_cache) {
  const DexFile* dex_file = dex_cache->GetDexFile();
  DCHECK_EQ(dex_file->GetLocationChecksum(), DexFileChecksum());
  DCHECK_EQ(dex_file->NumClassDefs(), NumClassDefs());
  for (size_t i = 0; i < dex_cache->NumResolvedTypes(); ++i) {
    mirror::Class* klass = dex_cache->GetResolvedType(i);
    if (klass == nullptr) {
      continue;
    }
    DCHECK(!klass->IsProxyClass());
    mirror::DexCache* klass_dex_cache = klass->GetDexCache();
    if (klass_dex_cache == dex_cache) {
      const size_t class_def_idx = klass->GetDexClassDefIndex();
      DCHECK(klass->IsResolved());
      CHECK_LT(class_def_idx, num_class_defs_);
      SetInBitmap(class_def_idx, &resolved_bitmap_[0]);
    }
  }
}

bool DexCacheProfileData::IsResolved(size_t class_def_index) const {
  DCHECK_LT(class_def_index, NumClassDefs());
  return TestBitmap(class_def_index, &resolved_bitmap_[0]);
}

size_t DexCacheProfileData::WriteToVector(std::vector<uint8_t>* out) const {
  DCHECK(out != nullptr);
  // Include null terminator.
  const size_t start_size = out->size();
  Write(out, checksum_);
  Write(out, num_class_defs_);
  const size_t bitmap_bytes = BitmapBytes(NumClassDefs());
  out->insert(out->end(), &resolved_bitmap_[0], &resolved_bitmap_[0] + bitmap_bytes);
  return out->size() - start_size;
}

std::unordered_set<std::string> DexCacheProfileData::GetClassDescriptors(
    const DexFile& dex_file) const {
  std::unordered_set<std::string> ret;
  DCHECK_EQ(dex_file.GetLocationChecksum(), DexFileChecksum());
  const uint32_t num_class_defs = dex_file.NumClassDefs();
  DCHECK_EQ(num_class_defs, NumClassDefs());
  for (uint32_t i = 0; i < num_class_defs; ++i) {
    if (IsResolved(i)) {
      const DexFile::TypeId& type_id = dex_file.GetTypeId(dex_file.GetClassDef(i).class_idx_);
      ret.insert(dex_file.GetTypeDescriptor(type_id));
    }
  }
  return ret;
}

DexCacheProfileData* DexCacheProfileData::ReadFromMemory(const uint8_t** in,
                                                         size_t* in_size,
                                                         std::string* error_msg) {
  uint32_t checksum = 0;
  if (!Read(in, in_size, checksum)) {
    *error_msg = "Failed to read profile checksum: input too short";
    return nullptr;
  }
  uint32_t num_class_defs = 0;
  if (!Read(in, in_size, num_class_defs)) {
    *error_msg = "Failed to read profile num_class_defs: input too short";
    return nullptr;
  }
  std::unique_ptr<DexCacheProfileData> ret(new DexCacheProfileData(checksum, num_class_defs));
  const size_t num_bytes = BitmapBytes(num_class_defs);
  if (*in_size < num_bytes) {
    *error_msg = StringPrintf("Failed to read bitmap due to truncated input, "
                                  "expected %zu bytes but got %zu for %u class defs",
                              *in_size,
                              num_bytes,
                              num_class_defs);
    return nullptr;
  }
  memcpy(&ret->resolved_bitmap_[0], *in, num_bytes);
  *in_size -= num_bytes;
  *in += num_bytes;
  return ret.release();
}

void ClassProfile::Collect() {
  // Loop through all the dex caches.
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  Thread* const self = Thread::Current();
  const uint64_t start_time = NanoTime();
  ReaderMutexLock mu(self, *class_linker->DexLock());
  for (const ClassLinker::DexCacheData& data : class_linker->GetDexCachesData()) {
    if (self->IsJWeakCleared(data.weak_root)) {
      continue;
    }
    mirror::DexCache* dex_cache = self->DecodeJObject(data.weak_root)->AsDexCache();
    DCHECK(dex_cache != nullptr);
    const DexFile* dex_file = dex_cache->GetDexFile();
    const std::string& location = dex_file->GetLocation();
    const size_t num_class_defs = dex_file->NumClassDefs();
    DexCacheProfileData* const dex_cache_data = AddOrGetProfile(location,
                                                                dex_file->GetLocationChecksum(),
                                                                num_class_defs);
    CHECK_EQ(dex_cache_data->NumClassDefs(), num_class_defs);
    // Use the resolved types, this will miss array classes.
    const size_t num_types = dex_file->NumTypeIds();
    VLOG(class_linker) << "Collecting class profile for dex file " << location
                       << " types=" << num_types << " class_defs=" << num_class_defs;
    dex_cache_data->Update(dex_cache);
  }
  LOG(INFO) << "Collecting class profile took " << PrettyDuration(NanoTime() - start_time);
}

ClassProfile::DexFileMap ClassProfile::GetDexFileMap() {
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();
  Thread* const self = Thread::Current();
  std::unordered_map<std::string, const DexFile*> location_to_dex_file;
  ScopedObjectAccess soa(self);
  ReaderMutexLock mu(self, *class_linker->DexLock());
  for (const ClassLinker::DexCacheData& data : class_linker->GetDexCachesData()) {
    if (!self->IsJWeakCleared(data.weak_root)) {
      mirror::DexCache* dex_cache = self->DecodeJObject(data.weak_root)->AsDexCache();
      const DexFile* dex_file = dex_cache->GetDexFile();
      // There could be duplicates if two dex files with the same location are mapped.
      location_to_dex_file.emplace(dex_file->GetLocation(), dex_file);
    }
  }
  return location_to_dex_file;
}

void ClassProfile::Dump(std::ostream& os) {
  ReaderMutexLock mu(Thread::Current(), *Locks::class_profile_lock_);
  DexFileMap location_to_dex_file(GetDexFileMap());
  for (const auto& pair : dex_caches_) {
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

    const DexCacheProfileData* data = pair.second.get();
    size_t resolved = 0;
    for (size_t i = 0; i < data->NumClassDefs(); ++i) {
      if (data->IsResolved(i)) {
        ++resolved;
        os << "Class " << i << ": resolved ";
        if (dex_file != nullptr) {
          const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
          const DexFile::TypeId& type_id = dex_file->GetTypeId(class_def.class_idx_);
          const char* descriptor = dex_file->GetTypeDescriptor(type_id);
          os << descriptor;
        } else {
          os << "unknown";
        }
        os << "\n";
      }
    }
    os << "Resolved=" << resolved << "\n";
  }
}

size_t ClassProfile::Serialize(std::vector<uint8_t>* out) {
  ReaderMutexLock mu(Thread::Current(), *Locks::class_profile_lock_);
  DCHECK(out != nullptr);
  Header header;
  Write(out, header);
  Write(out, static_cast<uint32_t>(dex_caches_.size()));
  for (const auto& pair : dex_caches_) {
    const std::string& location = pair.first;
    out->insert(out->end(),
                  reinterpret_cast<const uint8_t*>(location.c_str()),
                  reinterpret_cast<const uint8_t*>(location.c_str()) + location.length() + 1);
    pair.second->WriteToVector(out);
  }
  return 0;
}

bool ClassProfile::Deserialize(const uint8_t* in, size_t in_size, std::string* error_msg) {
  WriterMutexLock mu(Thread::Current(), *Locks::class_profile_lock_);
  DCHECK(error_msg != nullptr);
  Header header;
  if (!Read(&in, &in_size, header)) {
    *error_msg = "Failed to read header: input too short";
    return false;
  }
  if (header.version != Header::kCurrentVersion) {
    *error_msg = StringPrintf("Header version is %u, expected %u",
                              header.version,
                              Header::kCurrentVersion);
    return false;
  }
  uint32_t num_profiles;
  if (!Read(&in, &in_size, num_profiles)) {
    *error_msg = "Failed to read num_profiles: input is too short";
    return false;
  }
  for (size_t i = 0; i < num_profiles; ++i) {
    // Read name first.
    size_t length = 0;
    while (true) {
      if (length >= in_size) {
        *error_msg = "Failed to read profile name: input too short";
        return false;
      }
      if (in[length] == '\0') {
        break;
      }
      ++length;
    }
    std::string name(in, in + length);
    in += length + 1;  // Skip null terminator.
    in_size -= length - 1;  // Skip null terminator.

    DexCacheProfileData* profile = DexCacheProfileData::ReadFromMemory(&in,
                                                                       &in_size,
                                                                       error_msg);
    if (profile == nullptr) {
      return false;
    }
    dex_caches_.emplace(name, std::unique_ptr<DexCacheProfileData>(profile));
  }
  VLOG(class_linker) << "Deserialized " << dex_caches_.size() << " profiles";
  return true;
}

void ClassProfile::WriteToFile(const char* file_name ATTRIBUTE_UNUSED) {
  // TODO: Implement.
}

void ClassProfile::ReadFromFile() {
  // TODO: Implement.
}

std::unordered_set<std::string> ClassProfile::GetClassDescriptors() const {
  std::unordered_set<std::string> ret;
  DexFileMap location_to_dex_file(GetDexFileMap());
  ReaderMutexLock mu(Thread::Current(), *Locks::class_profile_lock_);
  for (const auto& pair : dex_caches_) {
    const std::string& dex_file_name = pair.first;
    auto found = location_to_dex_file.find(dex_file_name);
    if (found != location_to_dex_file.end()) {
      const DexFile* dex_file = found->second;
      DCHECK(dex_file != nullptr);
      const DexCacheProfileData* data = pair.second.get();
      DCHECK(data != nullptr);
      std::unordered_set<std::string> descriptors(data->GetClassDescriptors(*dex_file));
      ret.insert(descriptors.begin(), descriptors.end());
    }
  }
  return ret;
}

DexCacheProfileData* ClassProfile::AddOrGetProfile(const std::string& location,
                                                   uint32_t checksum,
                                                   uint32_t num_class_defs) {
  DexCacheProfileData* dex_cache_data;
  auto found = dex_caches_.find(location);
  if (found == dex_caches_.end()) {
    dex_cache_data = new DexCacheProfileData(checksum, num_class_defs);
    dex_caches_.emplace(location, std::unique_ptr<DexCacheProfileData>(dex_cache_data));
  } else {
    dex_cache_data = found->second.get();
  }
  return dex_cache_data;
}

}  // namespace art

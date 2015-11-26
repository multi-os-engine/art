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

#include "oat_writer.h"

#include <unistd.h>
#include <zlib.h>

#include "arch/arm64/instruction_set_features_arm64.h"
#include "art_method-inl.h"
#include "base/allocator.h"
#include "base/bit_vector.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "compiled_class.h"
#include "compiled_method.h"
#include "dex_file-inl.h"
#include "dex/verification_results.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "dwarf/method_debug_info.h"
#include "gc/space/image_space.h"
#include "gc/space/space.h"
#include "handle_scope-inl.h"
#include "image_writer.h"
#include "linker/output_stream.h"
#include "linker/relative_patcher.h"
#include "mirror/array.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/object-inl.h"
#include "oat_quick_method_header.h"
#include "os.h"
#include "safe_map.h"
#include "scoped_thread_state_change.h"
#include "type_lookup_table.h"
#include "utils/dex_cache_arrays_layout-inl.h"
#include "verifier/method_verifier.h"
#include "zip_archive.h"

namespace art {

class OatWriter::OatDexFile {
 public:
  OatDexFile(size_t offset, const char* dex_file_location);
  OatDexFile(OatDexFile&& src) = default;

  const char* GetLocation() {
    return reinterpret_cast<const char*>(dex_file_location_data_);
  }

  size_t SizeOf() const;
  bool Write(OatWriter* oat_writer, OutputStream* out, const DexFile& dex_file) const;

  size_t GetClassOffsetsRawSize() const {
    return class_offsets_.size() * sizeof(class_offsets_[0]);
  }

  // Offset of start of OatDexFile from beginning of OatHeader. It is
  // used to validate file position when writing.
  size_t offset_;
  size_t size_;

  // data to write
  uint32_t dex_file_location_size_;
  const uint8_t* dex_file_location_data_;
  uint32_t dex_file_location_checksum_;
  uint32_t dex_file_offset_;
  uint32_t class_offsets_offset_;
  uint32_t lookup_table_offset_;

  // Data to write to a separate section.
  dchecked_vector<uint32_t> class_offsets_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OatDexFile);
};

class OatWriter::OatClass {
 public:
  OatClass(size_t offset,
           const dchecked_vector<CompiledMethod*>& compiled_methods,
           uint32_t num_non_null_compiled_methods,
           mirror::Class::Status status);
  OatClass(OatClass&& src) = default;
  size_t GetOatMethodOffsetsOffsetFromOatHeader(size_t class_def_method_index_) const;
  size_t GetOatMethodOffsetsOffsetFromOatClass(size_t class_def_method_index_) const;
  size_t SizeOf() const;
  bool Write(OatWriter* oat_writer, OutputStream* out, const size_t file_offset) const;

  CompiledMethod* GetCompiledMethod(size_t class_def_method_index) const {
    return compiled_methods_[class_def_method_index];
  }

  // Offset of start of OatClass from beginning of OatHeader. It is
  // used to validate file position when writing.
  size_t offset_;

  // CompiledMethods for each class_def_method_index, or null if no method is available.
  dchecked_vector<CompiledMethod*> compiled_methods_;

  // Offset from OatClass::offset_ to the OatMethodOffsets for the
  // class_def_method_index. If 0, it means the corresponding
  // CompiledMethod entry in OatClass::compiled_methods_ should be
  // null and that the OatClass::type_ should be kOatClassBitmap.
  dchecked_vector<uint32_t> oat_method_offsets_offsets_from_oat_class_;

  // data to write

  static_assert(mirror::Class::Status::kStatusMax < (1 << 16),
                "class status won't fit in 16bits");
  int16_t status_;

  static_assert(OatClassType::kOatClassMax < (1 << 16), "oat_class type won't fit in 16bits");
  uint16_t type_;

  uint32_t method_bitmap_size_;

  // bit vector indexed by ClassDef method index. When
  // OatClassType::type_ is kOatClassBitmap, a set bit indicates the
  // method has an OatMethodOffsets in methods_offsets_, otherwise
  // the entry was ommited to save space. If OatClassType::type_ is
  // not is kOatClassBitmap, the bitmap will be null.
  std::unique_ptr<BitVector> method_bitmap_;

  // OatMethodOffsets and OatMethodHeaders for each CompiledMethod
  // present in the OatClass. Note that some may be missing if
  // OatClass::compiled_methods_ contains null values (and
  // oat_method_offsets_offsets_from_oat_class_ should contain 0
  // values in this case).
  dchecked_vector<OatMethodOffsets> method_offsets_;
  dchecked_vector<OatQuickMethodHeader> method_headers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OatClass);
};

#define DCHECK_OFFSET() \
  DCHECK_EQ(static_cast<off_t>(file_offset + relative_offset), out->Seek(0, kSeekCurrent)) \
    << "file_offset=" << file_offset << " relative_offset=" << relative_offset

#define DCHECK_OFFSET_() \
  DCHECK_EQ(static_cast<off_t>(file_offset + offset_), out->Seek(0, kSeekCurrent)) \
    << "file_offset=" << file_offset << " offset_=" << offset_

OatWriter::OatWriter(InstructionSet instruction_set,
                     const InstructionSetFeatures* instruction_set_features,
                     const ArrayRef<const char* const>& dex_file_locations,
                     bool compiling_boot_image,
                     uint32_t image_file_location_oat_checksum,
                     uintptr_t image_file_location_oat_begin,
                     int32_t image_patch_delta,
                     SafeMap<std::string, std::string>* key_value_store,
                     TimingLogger* timings)
  : write_state_(WriteState::kUninitialized),
    timings_(timings),
    compiler_driver_(nullptr),
    image_writer_(nullptr),
    compiling_boot_image_(compiling_boot_image),
    dex_files_(nullptr),
    size_(0u),
    bss_size_(0u),
    oat_data_offset_(0u),
    oat_header_(nullptr),
    size_dex_file_alignment_(0),
    size_executable_offset_alignment_(0),
    size_oat_header_(0),
    size_oat_header_key_value_store_(0),
    size_dex_file_(0),
    size_interpreter_to_interpreter_bridge_(0),
    size_interpreter_to_compiled_code_bridge_(0),
    size_jni_dlsym_lookup_(0),
    size_quick_generic_jni_trampoline_(0),
    size_quick_imt_conflict_trampoline_(0),
    size_quick_resolution_trampoline_(0),
    size_quick_to_interpreter_bridge_(0),
    size_trampoline_alignment_(0),
    size_method_header_(0),
    size_code_(0),
    size_code_alignment_(0),
    size_relative_call_thunks_(0),
    size_misc_thunks_(0),
    size_mapping_table_(0),
    size_vmap_table_(0),
    size_gc_map_(0),
    size_oat_dex_file_location_size_(0),
    size_oat_dex_file_location_data_(0),
    size_oat_dex_file_location_checksum_(0),
    size_oat_dex_file_offset_(0),
    size_oat_dex_file_class_offsets_offset_(0),
    size_oat_dex_file_lookup_table_offset_(0),
    size_oat_lookup_table_alignment_(0),
    size_oat_lookup_table_(0),
    size_oat_class_offsets_alignment_(0),
    size_oat_class_offsets_(0),
    size_oat_class_type_(0),
    size_oat_class_status_(0),
    size_oat_class_method_bitmaps_(0),
    size_oat_class_method_offsets_(0),
    method_offset_map_() {
  size_t offset;
  {
    TimingLogger::ScopedTiming split("InitOatHeader", timings_);
    offset = InitOatHeader(instruction_set,
                           instruction_set_features,
                           dchecked_integral_cast<uint32_t>(dex_file_locations.size()),
                           image_file_location_oat_checksum,
                           image_file_location_oat_begin,
                           image_patch_delta,
                           key_value_store);
    size_oat_header_ += sizeof(OatHeader);
    size_oat_header_key_value_store_ += oat_header_->GetHeaderSize() - sizeof(OatHeader);
  }

  {
    TimingLogger::ScopedTiming split("InitOatDexFiles", timings_);
    offset = InitOatDexFiles(offset, dex_file_locations);
  }
  size_ = offset;

  write_state_ = WriteState::kWriteDexFiles;
}

bool OatWriter::WriteDexFiles(OutputStream* rodata,
                              File* file,
                              const ArrayRef<RawDexFileLocation const>& dex_files) {
  CHECK(write_state_ == WriteState::kWriteDexFiles);
  CHECK_EQ(dex_files.size(), oat_header_->GetDexFileCount());

  TimingLogger::ScopedTiming split("WriteDexFiles", timings_);

  // Get the elf file offset of the oat file.
  if (!GetOatDataOffset(rodata)) {
    return false;
  }

  for (size_t i = 0, size = dex_files.size(); i != size; ++i) {
    OatDexFile* oat_dex_file = &oat_dex_files_[i];
    if (!SeekToDexFile(rodata, file, oat_dex_file)) {
      return false;
    }
    if (dex_files[i].zip_entry != nullptr) {
      CHECK(dex_files[i].raw_file == nullptr);
      if (!WriteDexFile(rodata, file, oat_dex_file, dex_files[i].zip_entry)) {
        return false;
      }
    } else {
      CHECK(dex_files[i].raw_file != nullptr);
      if (!WriteDexFile(rodata, file, oat_dex_file, dex_files[i].raw_file)) {
        return false;
      }
    }
  }

  write_state_ = WriteState::kWriteLookupTables;
  return true;
}

bool OatWriter::WriteDexFiles(OutputStream* rodata, const std::vector<const DexFile*>& dex_files) {
  CHECK(write_state_ == WriteState::kWriteDexFiles);
  CHECK_EQ(dex_files.size(), oat_header_->GetDexFileCount());

  TimingLogger::ScopedTiming split("WriteDexFiles", timings_);

  // Get the elf file offset of the oat file.
  const off_t raw_file_offset = rodata->Seek(0, kSeekCurrent);
  if (raw_file_offset == static_cast<off_t>(-1)) {
    LOG(ERROR) << "Failed to get file offset in " << rodata->GetLocation();
    return false;
  }
  oat_data_offset_ = static_cast<size_t>(raw_file_offset);

  for (size_t i = 0, size = dex_files.size(); i != size; ++i) {
    const DexFile& dex_file = *dex_files[i];
    OatDexFile* oat_dex_file = &oat_dex_files_[i];

    // Dex files are required to be 4 byte aligned.
    size_t original_offset = size_;
    size_t offset = RoundUp(original_offset, 4);
    size_dex_file_alignment_ += offset - original_offset;

    // Update the dex file offset and location checksum in the OatDexFile.
    oat_dex_file->dex_file_offset_ = offset;
    oat_dex_file->dex_file_location_checksum_ = dex_file.GetLocationChecksum();

    uint32_t expected_offset = oat_data_offset_ + offset;
    off_t actual_offset = rodata->Seek(expected_offset, kSeekSet);
    if (static_cast<uint32_t>(actual_offset) != expected_offset) {
      PLOG(ERROR) << "Failed to seek to dex file section. Actual: " << actual_offset
                  << " Expected: " << expected_offset << " File: " << dex_file.GetLocation();
      return false;
    }
    if (!rodata->WriteFully(&dex_file.GetHeader(), dex_file.GetHeader().file_size_)) {
      PLOG(ERROR) << "Failed to write dex file " << dex_file.GetLocation()
                  << " to " << rodata->GetLocation();
      return false;
    }
    size_dex_file_ += dex_file.GetHeader().file_size_;

    // Update size in the OatDexFile.
    oat_dex_file->size_ = offset;

    size_ = offset + dex_file.GetHeader().file_size_;
  }

  write_state_ = WriteState::kWriteLookupTables;
  return true;
}

bool OatWriter::WriteTypeLookupTables(OutputStream* rodata,
                                      const std::vector<const DexFile*>& dex_files) {
  CHECK(write_state_ == WriteState::kWriteLookupTables);
  CHECK_EQ(dex_files.size(), oat_header_->GetDexFileCount());

  for (size_t i = 0, size = dex_files.size(); i != size; ++i) {
    const DexFile& dex_file = *dex_files[i];
    OatDexFile* oat_dex_file = &oat_dex_files_[i];

    const TypeLookupTable* lookup_table = dex_file.GetTypeLookupTable();
    if (lookup_table != nullptr && lookup_table->RawDataLength() != 0u) {
      // Type tables are required to be 4 byte aligned.
      size_t original_offset = size_;
      size_t offset = RoundUp(original_offset, 4);
      size_oat_lookup_table_alignment_ += offset - original_offset;

      // Set the type lookup table offset in OatDexFile.
      oat_dex_file->lookup_table_offset_ = offset;

      uint32_t expected_offset = oat_data_offset_ + offset;
      off_t actual_offset = rodata->Seek(expected_offset, kSeekSet);
      if (static_cast<uint32_t>(actual_offset) != expected_offset) {
        PLOG(ERROR) << "Failed to seek to lookup table section. Actual: " << actual_offset
                    << " Expected: " << expected_offset << " File: " << dex_file.GetLocation();
        return false;
      }
      if (!WriteData(rodata, lookup_table->RawData(), lookup_table->RawDataLength())) {
        PLOG(ERROR) << "Failed to write lookup table for " << dex_file.GetLocation()
                    << " to " << rodata->GetLocation();
        return false;
      }
      size_oat_lookup_table_ += lookup_table->RawDataLength();

      size_ = offset + lookup_table->RawDataLength();
    } else {
      oat_dex_file->lookup_table_offset_ = 0u;
    }
  }

  write_state_ = WriteState::kWriteOatDexFiles;
  return true;
}

bool OatWriter::WriteOatDexFiles(OutputStream* rodata,
                                 const std::vector<const DexFile*>& dex_files) {
  CHECK(write_state_ == WriteState::kWriteOatDexFiles);
  CHECK_EQ(dex_files.size(), oat_dex_files_.size());

  dex_files_ = &dex_files;

  TimingLogger::ScopedTiming split("WriteOatDexFiles", timings_);

  // Seek to the start of OatDexFiles, i.e. to the end of the OatHeader.  If there are
  // no OatDexFiles, no data is actually written to .rodata before WriteHeader() and
  // this Seek() ensures that we reserve the space for OatHeader in .rodata.
  DCHECK(oat_dex_files_.empty() || oat_dex_files_[0u].offset_ == oat_header_->GetHeaderSize());
  uint32_t expected_offset = oat_data_offset_ + oat_header_->GetHeaderSize();
  off_t actual_offset = rodata->Seek(expected_offset, kSeekSet);
  if (static_cast<uint32_t>(actual_offset) != expected_offset) {
    PLOG(ERROR) << "Failed to seek to OatDexFile table section. Actual: " << actual_offset
                << " Expected: " << expected_offset << " File: " << dex_files[0]->GetLocation();
    return false;
  }

  for (size_t i = 0, size = oat_dex_files_.size(); i != size; ++i) {
    OatDexFile* oat_dex_file = &oat_dex_files_[i];
    const DexFile& dex_file = *dex_files[i];

    // Update OatDexFile.
    oat_dex_file->class_offsets_.resize(dex_file.NumClassDefs());
    DCHECK_EQ(oat_dex_file->class_offsets_offset_, 0u);
    if (dex_file.NumClassDefs() != 0u) {
      // Class offsets are required to be 4 byte aligned.
      size_t original_offset = size_;
      size_t offset = RoundUp(original_offset, 4);
      size_oat_class_offsets_alignment_ += offset - original_offset;
      oat_dex_file->class_offsets_offset_ = offset;
      size_ = offset + oat_dex_file->GetClassOffsetsRawSize();
    }

    DCHECK_EQ(oat_data_offset_ + oat_dex_file->offset_,
              static_cast<size_t>(rodata->Seek(0, kSeekCurrent)));

    // Write OatDexFile.
    if (!oat_dex_file->Write(this, rodata, dex_file)) {
      PLOG(ERROR) << "Failed to write oat dex information to " << rodata->GetLocation();
      return false;
    }
  }

  write_state_ = WriteState::kPrepareLayout;
  return true;
}

void OatWriter::PrepareLayout(const CompilerDriver* compiler,
                              ImageWriter* image_writer) {
  CHECK(write_state_ == WriteState::kPrepareLayout);

  compiler_driver_ = compiler;
  image_writer_ = image_writer;
  if (compiling_boot_image_) {
    CHECK(image_writer_ != nullptr);
  }
  InstructionSet instruction_set = compiler_driver_->GetInstructionSet();
  CHECK_EQ(instruction_set, oat_header_->GetInstructionSet());
  const InstructionSetFeatures* features = compiler_driver_->GetInstructionSetFeatures();
  relative_patcher_ = linker::RelativePatcher::Create(instruction_set, features,
                                                      &method_offset_map_);

  uint32_t offset = size_;
  {
    TimingLogger::ScopedTiming split("InitOatClasses", timings_);
    offset = InitOatClasses(offset);
  }
  {
    TimingLogger::ScopedTiming split("InitOatMaps", timings_);
    offset = InitOatMaps(offset);
  }
  {
    TimingLogger::ScopedTiming split("InitOatCode", timings_);
    offset = InitOatCode(offset);
  }
  {
    TimingLogger::ScopedTiming split("InitOatCodeDexFiles", timings_);
    offset = InitOatCodeDexFiles(offset);
  }
  size_ = offset;

  if (!HasBootImage()) {
    // Allocate space for app dex cache arrays in the .bss section.
    size_t bss_start = RoundUp(size_, kPageSize);
    size_t pointer_size = GetInstructionSetPointerSize(instruction_set);
    bss_size_ = 0u;
    for (const DexFile* dex_file : *dex_files_) {
      dex_cache_arrays_offsets_.Put(dex_file, bss_start + bss_size_);
      DexCacheArraysLayout layout(pointer_size, dex_file);
      bss_size_ += layout.Size();
    }
  }

  CHECK_EQ(dex_files_->size(), oat_dex_files_.size());
  if (compiling_boot_image_) {
    CHECK_EQ(image_writer_ != nullptr,
             oat_header_->GetStoreValueByKey(OatHeader::kImageLocationKey) == nullptr);
  }

  write_state_ = WriteState::kWriteRoData;
}

OatWriter::~OatWriter() {
}

struct OatWriter::GcMapDataAccess {
  static ArrayRef<const uint8_t> GetData(const CompiledMethod* compiled_method) ALWAYS_INLINE {
    return compiled_method->GetGcMap();
  }

  static uint32_t GetOffset(OatClass* oat_class, size_t method_offsets_index) ALWAYS_INLINE {
    uint32_t offset = oat_class->method_headers_[method_offsets_index].gc_map_offset_;
    return offset == 0u ? 0u :
        (oat_class->method_offsets_[method_offsets_index].code_offset_ & ~1) - offset;
  }

  static void SetOffset(OatClass* oat_class, size_t method_offsets_index, uint32_t offset)
      ALWAYS_INLINE {
    oat_class->method_headers_[method_offsets_index].gc_map_offset_ =
        (oat_class->method_offsets_[method_offsets_index].code_offset_ & ~1) - offset;
  }

  static const char* Name() {
    return "GC map";
  }
};

struct OatWriter::MappingTableDataAccess {
  static ArrayRef<const uint8_t> GetData(const CompiledMethod* compiled_method) ALWAYS_INLINE {
    return compiled_method->GetMappingTable();
  }

  static uint32_t GetOffset(OatClass* oat_class, size_t method_offsets_index) ALWAYS_INLINE {
    uint32_t offset = oat_class->method_headers_[method_offsets_index].mapping_table_offset_;
    return offset == 0u ? 0u :
        (oat_class->method_offsets_[method_offsets_index].code_offset_ & ~1) - offset;
  }

  static void SetOffset(OatClass* oat_class, size_t method_offsets_index, uint32_t offset)
      ALWAYS_INLINE {
    oat_class->method_headers_[method_offsets_index].mapping_table_offset_ =
        (oat_class->method_offsets_[method_offsets_index].code_offset_ & ~1) - offset;
  }

  static const char* Name() {
    return "mapping table";
  }
};

struct OatWriter::VmapTableDataAccess {
  static ArrayRef<const uint8_t> GetData(const CompiledMethod* compiled_method) ALWAYS_INLINE {
    return compiled_method->GetVmapTable();
  }

  static uint32_t GetOffset(OatClass* oat_class, size_t method_offsets_index) ALWAYS_INLINE {
    uint32_t offset = oat_class->method_headers_[method_offsets_index].vmap_table_offset_;
    return offset == 0u ? 0u :
        (oat_class->method_offsets_[method_offsets_index].code_offset_ & ~1) - offset;
  }

  static void SetOffset(OatClass* oat_class, size_t method_offsets_index, uint32_t offset)
      ALWAYS_INLINE {
    oat_class->method_headers_[method_offsets_index].vmap_table_offset_ =
        (oat_class->method_offsets_[method_offsets_index].code_offset_ & ~1) - offset;
  }

  static const char* Name() {
    return "vmap table";
  }
};

class OatWriter::DexMethodVisitor {
 public:
  DexMethodVisitor(OatWriter* writer, size_t offset)
    : writer_(writer),
      offset_(offset),
      dex_file_(nullptr),
      class_def_index_(DexFile::kDexNoIndex) {
  }

  virtual bool StartClass(const DexFile* dex_file, size_t class_def_index) {
    DCHECK(dex_file_ == nullptr);
    DCHECK_EQ(class_def_index_, DexFile::kDexNoIndex);
    dex_file_ = dex_file;
    class_def_index_ = class_def_index;
    return true;
  }

  virtual bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it) = 0;

  virtual bool EndClass() {
    if (kIsDebugBuild) {
      dex_file_ = nullptr;
      class_def_index_ = DexFile::kDexNoIndex;
    }
    return true;
  }

  size_t GetOffset() const {
    return offset_;
  }

 protected:
  virtual ~DexMethodVisitor() { }

  OatWriter* const writer_;

  // The offset is usually advanced for each visited method by the derived class.
  size_t offset_;

  // The dex file and class def index are set in StartClass().
  const DexFile* dex_file_;
  size_t class_def_index_;
};

class OatWriter::OatDexMethodVisitor : public DexMethodVisitor {
 public:
  OatDexMethodVisitor(OatWriter* writer, size_t offset)
    : DexMethodVisitor(writer, offset),
      oat_class_index_(0u),
      method_offsets_index_(0u) {
  }

  bool StartClass(const DexFile* dex_file, size_t class_def_index) {
    DexMethodVisitor::StartClass(dex_file, class_def_index);
    DCHECK_LT(oat_class_index_, writer_->oat_classes_.size());
    method_offsets_index_ = 0u;
    return true;
  }

  bool EndClass() {
    ++oat_class_index_;
    return DexMethodVisitor::EndClass();
  }

 protected:
  size_t oat_class_index_;
  size_t method_offsets_index_;
};

class OatWriter::InitOatClassesMethodVisitor : public DexMethodVisitor {
 public:
  InitOatClassesMethodVisitor(OatWriter* writer, size_t offset)
    : DexMethodVisitor(writer, offset),
      compiled_methods_(),
      num_non_null_compiled_methods_(0u) {
    size_t num_classes = 0u;
    for (const OatDexFile& oat_dex_file : writer_->oat_dex_files_) {
      num_classes += oat_dex_file.class_offsets_.size();
    }
    writer_->oat_classes_.reserve(num_classes);
    compiled_methods_.reserve(256u);
  }

  bool StartClass(const DexFile* dex_file, size_t class_def_index) {
    DexMethodVisitor::StartClass(dex_file, class_def_index);
    compiled_methods_.clear();
    num_non_null_compiled_methods_ = 0u;
    return true;
  }

  bool VisitMethod(size_t class_def_method_index ATTRIBUTE_UNUSED,
                   const ClassDataItemIterator& it) {
    // Fill in the compiled_methods_ array for methods that have a
    // CompiledMethod. We track the number of non-null entries in
    // num_non_null_compiled_methods_ since we only want to allocate
    // OatMethodOffsets for the compiled methods.
    uint32_t method_idx = it.GetMemberIndex();
    CompiledMethod* compiled_method =
        writer_->compiler_driver_->GetCompiledMethod(MethodReference(dex_file_, method_idx));
    compiled_methods_.push_back(compiled_method);
    if (compiled_method != nullptr) {
        ++num_non_null_compiled_methods_;
    }
    return true;
  }

  bool EndClass() {
    ClassReference class_ref(dex_file_, class_def_index_);
    CompiledClass* compiled_class = writer_->compiler_driver_->GetCompiledClass(class_ref);
    mirror::Class::Status status;
    if (compiled_class != nullptr) {
      status = compiled_class->GetStatus();
    } else if (writer_->compiler_driver_->GetVerificationResults()->IsClassRejected(class_ref)) {
      status = mirror::Class::kStatusError;
    } else {
      status = mirror::Class::kStatusNotReady;
    }

    writer_->oat_classes_.emplace_back(offset_,
                                       compiled_methods_,
                                       num_non_null_compiled_methods_,
                                       status);
    offset_ += writer_->oat_classes_.back().SizeOf();
    return DexMethodVisitor::EndClass();
  }

 private:
  dchecked_vector<CompiledMethod*> compiled_methods_;
  size_t num_non_null_compiled_methods_;
};

class OatWriter::InitCodeMethodVisitor : public OatDexMethodVisitor {
 public:
  InitCodeMethodVisitor(OatWriter* writer, size_t offset)
    : OatDexMethodVisitor(writer, offset),
      debuggable_(writer->GetCompilerDriver()->GetCompilerOptions().GetDebuggable()) {
    writer_->absolute_patch_locations_.reserve(
        writer_->compiler_driver_->GetNonRelativeLinkerPatchCount());
  }

  bool EndClass() {
    OatDexMethodVisitor::EndClass();
    if (oat_class_index_ == writer_->oat_classes_.size()) {
      offset_ = writer_->relative_patcher_->ReserveSpaceEnd(offset_);
    }
    return true;
  }

  bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    if (compiled_method != nullptr) {
      // Derived from CompiledMethod.
      uint32_t quick_code_offset = 0;

      ArrayRef<const uint8_t> quick_code = compiled_method->GetQuickCode();
      uint32_t code_size = quick_code.size() * sizeof(uint8_t);
      uint32_t thumb_offset = compiled_method->CodeDelta();

      // Deduplicate code arrays if we are not producing debuggable code.
      bool deduped = false;
      if (debuggable_) {
        quick_code_offset = NewQuickCodeOffset(compiled_method, it, thumb_offset);
      } else {
        auto lb = dedupe_map_.lower_bound(compiled_method);
        if (lb != dedupe_map_.end() && !dedupe_map_.key_comp()(compiled_method, lb->first)) {
          quick_code_offset = lb->second;
          deduped = true;
        } else {
          quick_code_offset = NewQuickCodeOffset(compiled_method, it, thumb_offset);
          dedupe_map_.PutBefore(lb, compiled_method, quick_code_offset);
        }
      }

      if (code_size != 0) {
        MethodReference method_ref(dex_file_, it.GetMemberIndex());
        auto method_lb = writer_->method_offset_map_.map.lower_bound(method_ref);
        if (method_lb != writer_->method_offset_map_.map.end() &&
            !writer_->method_offset_map_.map.key_comp()(method_ref, method_lb->first)) {
          // TODO: Should this be a hard failure?
          LOG(WARNING) << "Multiple definitions of "
              << PrettyMethod(method_ref.dex_method_index, *method_ref.dex_file)
              << ((method_lb->second != quick_code_offset) ? "; OFFSET MISMATCH" : "");
        } else {
          writer_->method_offset_map_.map.PutBefore(method_lb, method_ref, quick_code_offset);
        }
      }

      // Update quick method header.
      DCHECK_LT(method_offsets_index_, oat_class->method_headers_.size());
      OatQuickMethodHeader* method_header = &oat_class->method_headers_[method_offsets_index_];
      uint32_t mapping_table_offset = method_header->mapping_table_offset_;
      uint32_t vmap_table_offset = method_header->vmap_table_offset_;
      // If we don't have quick code, then we must have a vmap, as that is how the dex2dex
      // compiler records its transformations.
      DCHECK(!quick_code.empty() || vmap_table_offset != 0);
      uint32_t gc_map_offset = method_header->gc_map_offset_;
      // The code offset was 0 when the mapping/vmap table offset was set, so it's set
      // to 0-offset and we need to adjust it by code_offset.
      uint32_t code_offset = quick_code_offset - thumb_offset;
      if (mapping_table_offset != 0u && code_offset != 0u) {
        mapping_table_offset += code_offset;
        DCHECK_LT(mapping_table_offset, code_offset) << "Overflow in oat offsets";
      }
      if (vmap_table_offset != 0u && code_offset != 0u) {
        vmap_table_offset += code_offset;
        DCHECK_LT(vmap_table_offset, code_offset) << "Overflow in oat offsets";
      }
      if (gc_map_offset != 0u && code_offset != 0u) {
        gc_map_offset += code_offset;
        DCHECK_LT(gc_map_offset, code_offset) << "Overflow in oat offsets";
      }
      uint32_t frame_size_in_bytes = compiled_method->GetFrameSizeInBytes();
      uint32_t core_spill_mask = compiled_method->GetCoreSpillMask();
      uint32_t fp_spill_mask = compiled_method->GetFpSpillMask();
      *method_header = OatQuickMethodHeader(mapping_table_offset, vmap_table_offset,
                                            gc_map_offset, frame_size_in_bytes, core_spill_mask,
                                            fp_spill_mask, code_size);

      if (!deduped) {
        // Update offsets. (Checksum is updated when writing.)
        offset_ += sizeof(*method_header);  // Method header is prepended before code.
        offset_ += code_size;
        // Record absolute patch locations.
        if (!compiled_method->GetPatches().empty()) {
          uintptr_t base_loc = offset_ - code_size - writer_->oat_header_->GetExecutableOffset();
          for (const LinkerPatch& patch : compiled_method->GetPatches()) {
            if (!patch.IsPcRelative()) {
              writer_->absolute_patch_locations_.push_back(base_loc + patch.LiteralOffset());
            }
          }
        }
      }

      if (writer_->compiler_driver_->GetCompilerOptions().GetGenerateDebugInfo()) {
        // Record debug information for this function if we are doing that.
        const uint32_t quick_code_start = quick_code_offset -
            writer_->oat_header_->GetExecutableOffset() - thumb_offset;
        writer_->method_info_.push_back(dwarf::MethodDebugInfo {
            dex_file_,
            class_def_index_,
            it.GetMemberIndex(),
            it.GetMethodAccessFlags(),
            it.GetMethodCodeItem(),
            deduped,
            quick_code_start,
            quick_code_start + code_size,
            compiled_method});
      }

      if (kIsDebugBuild) {
        // We expect GC maps except when the class hasn't been verified or the method is native.
        const CompilerDriver* compiler_driver = writer_->compiler_driver_;
        ClassReference class_ref(dex_file_, class_def_index_);
        CompiledClass* compiled_class = compiler_driver->GetCompiledClass(class_ref);
        mirror::Class::Status status;
        if (compiled_class != nullptr) {
          status = compiled_class->GetStatus();
        } else if (compiler_driver->GetVerificationResults()->IsClassRejected(class_ref)) {
          status = mirror::Class::kStatusError;
        } else {
          status = mirror::Class::kStatusNotReady;
        }
        ArrayRef<const uint8_t> gc_map = compiled_method->GetGcMap();
        if (!gc_map.empty()) {
          size_t gc_map_size = gc_map.size() * sizeof(gc_map[0]);
          bool is_native = it.MemberIsNative();
          CHECK(gc_map_size != 0 || is_native || status < mirror::Class::kStatusVerified)
              << gc_map_size << " " << (is_native ? "true" : "false") << " "
              << (status < mirror::Class::kStatusVerified) << " " << status << " "
              << PrettyMethod(it.GetMemberIndex(), *dex_file_);
        }
      }

      DCHECK_LT(method_offsets_index_, oat_class->method_offsets_.size());
      OatMethodOffsets* offsets = &oat_class->method_offsets_[method_offsets_index_];
      offsets->code_offset_ = quick_code_offset;
      ++method_offsets_index_;
    }

    return true;
  }

 private:
  struct CodeOffsetsKeyComparator {
    bool operator()(const CompiledMethod* lhs, const CompiledMethod* rhs) const {
      // Code is deduplicated by CompilerDriver, compare only data pointers.
      if (lhs->GetQuickCode().data() != rhs->GetQuickCode().data()) {
        return lhs->GetQuickCode().data() < rhs->GetQuickCode().data();
      }
      // If the code is the same, all other fields are likely to be the same as well.
      if (UNLIKELY(lhs->GetMappingTable().data() != rhs->GetMappingTable().data())) {
        return lhs->GetMappingTable().data() < rhs->GetMappingTable().data();
      }
      if (UNLIKELY(lhs->GetVmapTable().data() != rhs->GetVmapTable().data())) {
        return lhs->GetVmapTable().data() < rhs->GetVmapTable().data();
      }
      if (UNLIKELY(lhs->GetGcMap().data() != rhs->GetGcMap().data())) {
        return lhs->GetGcMap().data() < rhs->GetGcMap().data();
      }
      if (UNLIKELY(lhs->GetPatches().data() != rhs->GetPatches().data())) {
        return lhs->GetPatches().data() < rhs->GetPatches().data();
      }
      return false;
    }
  };

  uint32_t NewQuickCodeOffset(CompiledMethod* compiled_method,
                              const ClassDataItemIterator& it,
                              uint32_t thumb_offset) {
    offset_ = writer_->relative_patcher_->ReserveSpace(
        offset_, compiled_method, MethodReference(dex_file_, it.GetMemberIndex()));
    offset_ = compiled_method->AlignCode(offset_);
    DCHECK_ALIGNED_PARAM(offset_,
                         GetInstructionSetAlignment(compiled_method->GetInstructionSet()));
    return offset_ + sizeof(OatQuickMethodHeader) + thumb_offset;
  }

  // Deduplication is already done on a pointer basis by the compiler driver,
  // so we can simply compare the pointers to find out if things are duplicated.
  SafeMap<const CompiledMethod*, uint32_t, CodeOffsetsKeyComparator> dedupe_map_;

  // Cache of compiler's --debuggable option.
  const bool debuggable_;
};

template <typename DataAccess>
class OatWriter::InitMapMethodVisitor : public OatDexMethodVisitor {
 public:
  InitMapMethodVisitor(OatWriter* writer, size_t offset)
    : OatDexMethodVisitor(writer, offset) {
  }

  bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it ATTRIBUTE_UNUSED)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    if (compiled_method != nullptr) {
      DCHECK_LT(method_offsets_index_, oat_class->method_offsets_.size());
      DCHECK_EQ(DataAccess::GetOffset(oat_class, method_offsets_index_), 0u);

      ArrayRef<const uint8_t> map = DataAccess::GetData(compiled_method);
      uint32_t map_size = map.size() * sizeof(map[0]);
      if (map_size != 0u) {
        auto lb = dedupe_map_.lower_bound(map.data());
        if (lb != dedupe_map_.end() && !dedupe_map_.key_comp()(map.data(), lb->first)) {
          DataAccess::SetOffset(oat_class, method_offsets_index_, lb->second);
        } else {
          DataAccess::SetOffset(oat_class, method_offsets_index_, offset_);
          dedupe_map_.PutBefore(lb, map.data(), offset_);
          offset_ += map_size;
        }
      }
      ++method_offsets_index_;
    }

    return true;
  }

 private:
  // Deduplication is already done on a pointer basis by the compiler driver,
  // so we can simply compare the pointers to find out if things are duplicated.
  SafeMap<const uint8_t*, uint32_t> dedupe_map_;
};

class OatWriter::InitImageMethodVisitor : public OatDexMethodVisitor {
 public:
  InitImageMethodVisitor(OatWriter* writer, size_t offset)
    : OatDexMethodVisitor(writer, offset),
      pointer_size_(GetInstructionSetPointerSize(writer_->compiler_driver_->GetInstructionSet())) {
  }

  bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    OatMethodOffsets offsets(0u);
    if (compiled_method != nullptr) {
      DCHECK_LT(method_offsets_index_, oat_class->method_offsets_.size());
      offsets = oat_class->method_offsets_[method_offsets_index_];
      ++method_offsets_index_;
    }

    ClassLinker* linker = Runtime::Current()->GetClassLinker();
    InvokeType invoke_type = it.GetMethodInvokeType(dex_file_->GetClassDef(class_def_index_));
    // Unchecked as we hold mutator_lock_ on entry.
    ScopedObjectAccessUnchecked soa(Thread::Current());
    StackHandleScope<1> hs(soa.Self());
    Handle<mirror::DexCache> dex_cache(hs.NewHandle(linker->FindDexCache(
        Thread::Current(), *dex_file_)));
    ArtMethod* method = linker->ResolveMethod(
        *dex_file_, it.GetMemberIndex(), dex_cache, NullHandle<mirror::ClassLoader>(), nullptr,
        invoke_type);
    if (method == nullptr) {
      LOG(INTERNAL_FATAL) << "Unexpected failure to resolve a method: "
                          << PrettyMethod(it.GetMemberIndex(), *dex_file_, true);
      soa.Self()->AssertPendingException();
      mirror::Throwable* exc = soa.Self()->GetException();
      std::string dump = exc->Dump();
      LOG(FATAL) << dump;
      UNREACHABLE();
    }

    if (compiled_method != nullptr && compiled_method->GetQuickCode().size() != 0) {
      method->SetEntryPointFromQuickCompiledCodePtrSize(
          reinterpret_cast<void*>(offsets.code_offset_), pointer_size_);
    }

    return true;
  }

 protected:
  const size_t pointer_size_;
};

class OatWriter::WriteCodeMethodVisitor : public OatDexMethodVisitor {
 public:
  WriteCodeMethodVisitor(OatWriter* writer, OutputStream* out, const size_t file_offset,
                         size_t relative_offset) SHARED_LOCK_FUNCTION(Locks::mutator_lock_)
    : OatDexMethodVisitor(writer, relative_offset),
      out_(out),
      file_offset_(file_offset),
      soa_(Thread::Current()),
      no_thread_suspension_(soa_.Self(), "OatWriter patching"),
      class_linker_(Runtime::Current()->GetClassLinker()),
      dex_cache_(nullptr) {
    patched_code_.reserve(16 * KB);
    if (writer_->HasBootImage()) {
      // If we're creating the image, the address space must be ready so that we can apply patches.
      CHECK(writer_->image_writer_->IsImageAddressSpaceReady());
    }
  }

  ~WriteCodeMethodVisitor() UNLOCK_FUNCTION(Locks::mutator_lock_) {
  }

  bool StartClass(const DexFile* dex_file, size_t class_def_index)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    OatDexMethodVisitor::StartClass(dex_file, class_def_index);
    if (dex_cache_ == nullptr || dex_cache_->GetDexFile() != dex_file) {
      dex_cache_ = class_linker_->FindDexCache(Thread::Current(), *dex_file);
    }
    return true;
  }

  bool EndClass() SHARED_REQUIRES(Locks::mutator_lock_) {
    bool result = OatDexMethodVisitor::EndClass();
    if (oat_class_index_ == writer_->oat_classes_.size()) {
      DCHECK(result);  // OatDexMethodVisitor::EndClass() never fails.
      offset_ = writer_->relative_patcher_->WriteThunks(out_, offset_);
      if (UNLIKELY(offset_ == 0u)) {
        PLOG(ERROR) << "Failed to write final relative call thunks";
        result = false;
      }
    }
    return result;
  }

  bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    const CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    // No thread suspension since dex_cache_ that may get invalidated if that occurs.
    ScopedAssertNoThreadSuspension tsc(Thread::Current(), __FUNCTION__);
    if (compiled_method != nullptr) {  // ie. not an abstract method
      size_t file_offset = file_offset_;
      OutputStream* out = out_;

      ArrayRef<const uint8_t> quick_code = compiled_method->GetQuickCode();
      uint32_t code_size = quick_code.size() * sizeof(uint8_t);

      // Deduplicate code arrays.
      const OatMethodOffsets& method_offsets = oat_class->method_offsets_[method_offsets_index_];
      if (method_offsets.code_offset_ > offset_) {
        offset_ = writer_->relative_patcher_->WriteThunks(out, offset_);
        if (offset_ == 0u) {
          ReportWriteFailure("relative call thunk", it);
          return false;
        }
        uint32_t aligned_offset = compiled_method->AlignCode(offset_);
        uint32_t aligned_code_delta = aligned_offset - offset_;
        if (aligned_code_delta != 0) {
          if (!writer_->WriteCodeAlignment(out, aligned_code_delta)) {
            ReportWriteFailure("code alignment padding", it);
            return false;
          }
          offset_ += aligned_code_delta;
          DCHECK_OFFSET_();
        }
        DCHECK_ALIGNED_PARAM(offset_,
                             GetInstructionSetAlignment(compiled_method->GetInstructionSet()));
        DCHECK_EQ(method_offsets.code_offset_,
                  offset_ + sizeof(OatQuickMethodHeader) + compiled_method->CodeDelta())
            << PrettyMethod(it.GetMemberIndex(), *dex_file_);
        const OatQuickMethodHeader& method_header =
            oat_class->method_headers_[method_offsets_index_];
        if (!writer_->WriteData(out, &method_header, sizeof(method_header))) {
          ReportWriteFailure("method header", it);
          return false;
        }
        writer_->size_method_header_ += sizeof(method_header);
        offset_ += sizeof(method_header);
        DCHECK_OFFSET_();

        if (!compiled_method->GetPatches().empty()) {
          patched_code_.assign(quick_code.begin(), quick_code.end());
          quick_code = ArrayRef<const uint8_t>(patched_code_);
          for (const LinkerPatch& patch : compiled_method->GetPatches()) {
            if (patch.Type() == kLinkerPatchCallRelative) {
              // NOTE: Relative calls across oat files are not supported.
              uint32_t target_offset = GetTargetOffset(patch);
              uint32_t literal_offset = patch.LiteralOffset();
              writer_->relative_patcher_->PatchCall(&patched_code_, literal_offset,
                                                     offset_ + literal_offset, target_offset);
            } else if (patch.Type() == kLinkerPatchDexCacheArray) {
              uint32_t target_offset = GetDexCacheOffset(patch);
              uint32_t literal_offset = patch.LiteralOffset();
              writer_->relative_patcher_->PatchDexCacheReference(&patched_code_, patch,
                                                                 offset_ + literal_offset,
                                                                 target_offset);
            } else if (patch.Type() == kLinkerPatchCall) {
              uint32_t target_offset = GetTargetOffset(patch);
              PatchCodeAddress(&patched_code_, patch.LiteralOffset(), target_offset);
            } else if (patch.Type() == kLinkerPatchMethod) {
              ArtMethod* method = GetTargetMethod(patch);
              PatchMethodAddress(&patched_code_, patch.LiteralOffset(), method);
            } else if (patch.Type() == kLinkerPatchType) {
              mirror::Class* type = GetTargetType(patch);
              PatchObjectAddress(&patched_code_, patch.LiteralOffset(), type);
            }
          }
        }

        if (!writer_->WriteData(out, quick_code.data(), code_size)) {
          ReportWriteFailure("method code", it);
          return false;
        }
        writer_->size_code_ += code_size;
        offset_ += code_size;
      }
      DCHECK_OFFSET_();
      ++method_offsets_index_;
    }

    return true;
  }

 private:
  OutputStream* const out_;
  const size_t file_offset_;
  const ScopedObjectAccess soa_;
  const ScopedAssertNoThreadSuspension no_thread_suspension_;
  ClassLinker* const class_linker_;
  mirror::DexCache* dex_cache_;
  std::vector<uint8_t> patched_code_;

  void ReportWriteFailure(const char* what, const ClassDataItemIterator& it) {
    PLOG(ERROR) << "Failed to write " << what << " for "
        << PrettyMethod(it.GetMemberIndex(), *dex_file_) << " to " << out_->GetLocation();
  }

  ArtMethod* GetTargetMethod(const LinkerPatch& patch)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    MethodReference ref = patch.TargetMethod();
    mirror::DexCache* dex_cache =
        (dex_file_ == ref.dex_file) ? dex_cache_ : class_linker_->FindDexCache(
            Thread::Current(), *ref.dex_file);
    ArtMethod* method = dex_cache->GetResolvedMethod(
        ref.dex_method_index, class_linker_->GetImagePointerSize());
    CHECK(method != nullptr);
    return method;
  }

  uint32_t GetTargetOffset(const LinkerPatch& patch) SHARED_REQUIRES(Locks::mutator_lock_) {
    auto target_it = writer_->method_offset_map_.map.find(patch.TargetMethod());
    uint32_t target_offset =
        (target_it != writer_->method_offset_map_.map.end()) ? target_it->second : 0u;
    // If there's no compiled code, point to the correct trampoline.
    if (UNLIKELY(target_offset == 0)) {
      ArtMethod* target = GetTargetMethod(patch);
      DCHECK(target != nullptr);
      size_t size = GetInstructionSetPointerSize(writer_->compiler_driver_->GetInstructionSet());
      const void* oat_code_offset = target->GetEntryPointFromQuickCompiledCodePtrSize(size);
      if (oat_code_offset != 0) {
        DCHECK(!Runtime::Current()->GetClassLinker()->IsQuickResolutionStub(oat_code_offset));
        DCHECK(!Runtime::Current()->GetClassLinker()->IsQuickToInterpreterBridge(oat_code_offset));
        DCHECK(!Runtime::Current()->GetClassLinker()->IsQuickGenericJniStub(oat_code_offset));
        target_offset = PointerToLowMemUInt32(oat_code_offset);
      } else {
        target_offset = target->IsNative()
            ? writer_->oat_header_->GetQuickGenericJniTrampolineOffset()
            : writer_->oat_header_->GetQuickToInterpreterBridgeOffset();
      }
    }
    return target_offset;
  }

  mirror::Class* GetTargetType(const LinkerPatch& patch)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    mirror::DexCache* dex_cache = (dex_file_ == patch.TargetTypeDexFile())
        ? dex_cache_ : class_linker_->FindDexCache(Thread::Current(), *patch.TargetTypeDexFile());
    mirror::Class* type = dex_cache->GetResolvedType(patch.TargetTypeIndex());
    CHECK(type != nullptr);
    return type;
  }

  uint32_t GetDexCacheOffset(const LinkerPatch& patch) SHARED_REQUIRES(Locks::mutator_lock_) {
    if (writer_->HasBootImage()) {
      auto* element = writer_->image_writer_->GetDexCacheArrayElementImageAddress<const uint8_t*>(
              patch.TargetDexCacheDexFile(), patch.TargetDexCacheElementOffset());
      const uint8_t* oat_data = writer_->image_writer_->GetOatFileBegin() + file_offset_;
      return element - oat_data;
    } else {
      size_t start = writer_->dex_cache_arrays_offsets_.Get(patch.TargetDexCacheDexFile());
      return start + patch.TargetDexCacheElementOffset();
    }
  }

  void PatchObjectAddress(std::vector<uint8_t>* code, uint32_t offset, mirror::Object* object)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    if (writer_->HasBootImage()) {
      object = writer_->image_writer_->GetImageAddress(object);
    } else {
      // NOTE: We're using linker patches for app->boot references when the image can
      // be relocated and therefore we need to emit .oat_patches. We're not using this
      // for app->app references, so check that the object is in the image space.
      DCHECK(Runtime::Current()->GetHeap()->FindSpaceFromObject(object, false)->IsImageSpace());
    }
    // Note: We only patch targeting Objects in image which is in the low 4gb.
    uint32_t address = PointerToLowMemUInt32(object);
    DCHECK_LE(offset + 4, code->size());
    uint8_t* data = &(*code)[offset];
    data[0] = address & 0xffu;
    data[1] = (address >> 8) & 0xffu;
    data[2] = (address >> 16) & 0xffu;
    data[3] = (address >> 24) & 0xffu;
  }

  void PatchMethodAddress(std::vector<uint8_t>* code, uint32_t offset, ArtMethod* method)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    if (writer_->HasBootImage()) {
      method = writer_->image_writer_->GetImageMethodAddress(method);
    } else if (kIsDebugBuild) {
      // NOTE: We're using linker patches for app->boot references when the image can
      // be relocated and therefore we need to emit .oat_patches. We're not using this
      // for app->app references, so check that the method is an image method.
      gc::space::ImageSpace* image_space = Runtime::Current()->GetHeap()->GetBootImageSpace();
      size_t method_offset = reinterpret_cast<const uint8_t*>(method) - image_space->Begin();
      CHECK(image_space->GetImageHeader().GetMethodsSection().Contains(method_offset));
    }
    // Note: We only patch targeting ArtMethods in image which is in the low 4gb.
    uint32_t address = PointerToLowMemUInt32(method);
    DCHECK_LE(offset + 4, code->size());
    uint8_t* data = &(*code)[offset];
    data[0] = address & 0xffu;
    data[1] = (address >> 8) & 0xffu;
    data[2] = (address >> 16) & 0xffu;
    data[3] = (address >> 24) & 0xffu;
  }

  void PatchCodeAddress(std::vector<uint8_t>* code, uint32_t offset, uint32_t target_offset)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    uint32_t address = target_offset;
    if (writer_->HasBootImage()) {
      address = PointerToLowMemUInt32(writer_->image_writer_->GetOatFileBegin() +
                                      writer_->oat_data_offset_ + target_offset);
    }
    DCHECK_LE(offset + 4, code->size());
    uint8_t* data = &(*code)[offset];
    data[0] = address & 0xffu;
    data[1] = (address >> 8) & 0xffu;
    data[2] = (address >> 16) & 0xffu;
    data[3] = (address >> 24) & 0xffu;
  }
};

template <typename DataAccess>
class OatWriter::WriteMapMethodVisitor : public OatDexMethodVisitor {
 public:
  WriteMapMethodVisitor(OatWriter* writer, OutputStream* out, const size_t file_offset,
                        size_t relative_offset)
    : OatDexMethodVisitor(writer, relative_offset),
      out_(out),
      file_offset_(file_offset) {
  }

  bool VisitMethod(size_t class_def_method_index, const ClassDataItemIterator& it) {
    OatClass* oat_class = &writer_->oat_classes_[oat_class_index_];
    const CompiledMethod* compiled_method = oat_class->GetCompiledMethod(class_def_method_index);

    if (compiled_method != nullptr) {  // ie. not an abstract method
      size_t file_offset = file_offset_;
      OutputStream* out = out_;

      uint32_t map_offset = DataAccess::GetOffset(oat_class, method_offsets_index_);
      ++method_offsets_index_;

      // Write deduplicated map.
      ArrayRef<const uint8_t> map = DataAccess::GetData(compiled_method);
      size_t map_size = map.size() * sizeof(map[0]);
      DCHECK((map_size == 0u && map_offset == 0u) ||
            (map_size != 0u && map_offset != 0u && map_offset <= offset_))
          << map_size << " " << map_offset << " " << offset_ << " "
          << PrettyMethod(it.GetMemberIndex(), *dex_file_) << " for " << DataAccess::Name();
      if (map_size != 0u && map_offset == offset_) {
        if (UNLIKELY(!writer_->WriteData(out, map.data(), map_size))) {
          ReportWriteFailure(it);
          return false;
        }
        offset_ += map_size;
      }
      DCHECK_OFFSET_();
    }

    return true;
  }

 private:
  OutputStream* const out_;
  size_t const file_offset_;

  void ReportWriteFailure(const ClassDataItemIterator& it) {
    PLOG(ERROR) << "Failed to write " << DataAccess::Name() << " for "
        << PrettyMethod(it.GetMemberIndex(), *dex_file_) << " to " << out_->GetLocation();
  }
};

// Visit all methods from all classes in all dex files with the specified visitor.
bool OatWriter::VisitDexMethods(DexMethodVisitor* visitor) {
  for (const DexFile* dex_file : *dex_files_) {
    const size_t class_def_count = dex_file->NumClassDefs();
    for (size_t class_def_index = 0; class_def_index != class_def_count; ++class_def_index) {
      if (UNLIKELY(!visitor->StartClass(dex_file, class_def_index))) {
        return false;
      }
      const DexFile::ClassDef& class_def = dex_file->GetClassDef(class_def_index);
      const uint8_t* class_data = dex_file->GetClassData(class_def);
      if (class_data != nullptr) {  // ie not an empty class, such as a marker interface
        ClassDataItemIterator it(*dex_file, class_data);
        while (it.HasNextStaticField()) {
          it.Next();
        }
        while (it.HasNextInstanceField()) {
          it.Next();
        }
        size_t class_def_method_index = 0u;
        while (it.HasNextDirectMethod()) {
          if (!visitor->VisitMethod(class_def_method_index, it)) {
            return false;
          }
          ++class_def_method_index;
          it.Next();
        }
        while (it.HasNextVirtualMethod()) {
          if (UNLIKELY(!visitor->VisitMethod(class_def_method_index, it))) {
            return false;
          }
          ++class_def_method_index;
          it.Next();
        }
      }
      if (UNLIKELY(!visitor->EndClass())) {
        return false;
      }
    }
  }
  return true;
}

size_t OatWriter::InitOatHeader(InstructionSet instruction_set,
                                const InstructionSetFeatures* instruction_set_features,
                                uint32_t num_dex_files,
                                uint32_t image_file_location_oat_checksum,
                                uintptr_t image_file_location_oat_begin,
                                int32_t image_patch_delta,
                                SafeMap<std::string, std::string>* key_value_store) {
  oat_header_.reset(OatHeader::Create(instruction_set,
                                      instruction_set_features,
                                      num_dex_files,
                                      image_file_location_oat_checksum,
                                      image_file_location_oat_begin,
                                      key_value_store));
  CHECK_ALIGNED(image_patch_delta, kPageSize);
  oat_header_->SetImagePatchDelta(image_patch_delta);
  return oat_header_->GetHeaderSize();
}

size_t OatWriter::InitOatDexFiles(size_t offset,
                                  const ArrayRef<const char* const>& dex_file_locations) {
  // Create the OatDexFiles.
  oat_dex_files_.reserve(dex_file_locations.size());
  for (const char* location : dex_file_locations) {
    oat_dex_files_.emplace_back(offset, location);
    offset += oat_dex_files_.back().SizeOf();
  }
  return offset;
}

size_t OatWriter::InitOatClasses(size_t offset) {
  // calculate the offsets within OatDexFiles to OatClasses
  InitOatClassesMethodVisitor visitor(this, offset);
  bool success = VisitDexMethods(&visitor);
  CHECK(success);
  offset = visitor.GetOffset();

  // Update oat_dex_files_.
  auto oat_class_it = oat_classes_.begin();
  for (OatDexFile& oat_dex_file : oat_dex_files_) {
    for (uint32_t& class_offset : oat_dex_file.class_offsets_) {
      DCHECK(oat_class_it != oat_classes_.end());
      class_offset = oat_class_it->offset_;
      ++oat_class_it;
    }
  }
  CHECK(oat_class_it == oat_classes_.end());

  return offset;
}

size_t OatWriter::InitOatMaps(size_t offset) {
  #define VISIT(VisitorType)                          \
    do {                                              \
      VisitorType visitor(this, offset);              \
      bool success = VisitDexMethods(&visitor);       \
      DCHECK(success);                                \
      offset = visitor.GetOffset();                   \
    } while (false)

  VISIT(InitMapMethodVisitor<GcMapDataAccess>);
  VISIT(InitMapMethodVisitor<MappingTableDataAccess>);
  VISIT(InitMapMethodVisitor<VmapTableDataAccess>);

  #undef VISIT

  return offset;
}

size_t OatWriter::InitOatCode(size_t offset) {
  // calculate the offsets within OatHeader to executable code
  size_t old_offset = offset;
  size_t adjusted_offset = offset;
  // required to be on a new page boundary
  offset = RoundUp(offset, kPageSize);
  oat_header_->SetExecutableOffset(offset);
  size_executable_offset_alignment_ = offset - old_offset;
  if (compiler_driver_->IsBootImage()) {
    CHECK_EQ(oat_header_->GetImagePatchDelta(), 0);
    InstructionSet instruction_set = compiler_driver_->GetInstructionSet();

    #define DO_TRAMPOLINE(field, fn_name) \
      offset = CompiledCode::AlignCode(offset, instruction_set); \
      adjusted_offset = offset + CompiledCode::CodeDelta(instruction_set); \
      oat_header_->Set ## fn_name ## Offset(adjusted_offset); \
      field.reset(compiler_driver_->Create ## fn_name()); \
      offset += field->size();

    DO_TRAMPOLINE(jni_dlsym_lookup_, JniDlsymLookup);
    DO_TRAMPOLINE(quick_generic_jni_trampoline_, QuickGenericJniTrampoline);
    DO_TRAMPOLINE(quick_imt_conflict_trampoline_, QuickImtConflictTrampoline);
    DO_TRAMPOLINE(quick_resolution_trampoline_, QuickResolutionTrampoline);
    DO_TRAMPOLINE(quick_to_interpreter_bridge_, QuickToInterpreterBridge);

    #undef DO_TRAMPOLINE
  } else {
    oat_header_->SetInterpreterToInterpreterBridgeOffset(0);
    oat_header_->SetInterpreterToCompiledCodeBridgeOffset(0);
    oat_header_->SetJniDlsymLookupOffset(0);
    oat_header_->SetQuickGenericJniTrampolineOffset(0);
    oat_header_->SetQuickImtConflictTrampolineOffset(0);
    oat_header_->SetQuickResolutionTrampolineOffset(0);
    oat_header_->SetQuickToInterpreterBridgeOffset(0);
  }
  return offset;
}

size_t OatWriter::InitOatCodeDexFiles(size_t offset) {
  #define VISIT(VisitorType)                          \
    do {                                              \
      VisitorType visitor(this, offset);              \
      bool success = VisitDexMethods(&visitor);       \
      DCHECK(success);                                \
      offset = visitor.GetOffset();                   \
    } while (false)

  VISIT(InitCodeMethodVisitor);
  if (compiler_driver_->IsBootImage()) {
    VISIT(InitImageMethodVisitor);
  }

  #undef VISIT

  return offset;
}

bool OatWriter::WriteRodata(OutputStream* out) {
  CHECK(write_state_ == WriteState::kWriteRoData);

  if (!WriteClassOffsets(out)) {
    LOG(ERROR) << "Failed to write class offsets to " << out->GetLocation();
    return false;
  }

  if (!WriteClasses(out)) {
    LOG(ERROR) << "Failed to write classes to " << out->GetLocation();
    return false;
  }

  off_t tables_end_offset = out->Seek(0, kSeekCurrent);
  if (tables_end_offset == static_cast<off_t>(-1)) {
    LOG(ERROR) << "Failed to seek to oat code position in " << out->GetLocation();
    return false;
  }
  size_t file_offset = oat_data_offset_;
  size_t relative_offset = static_cast<size_t>(tables_end_offset) - file_offset;
  relative_offset = WriteMaps(out, file_offset, relative_offset);
  if (relative_offset == 0) {
    LOG(ERROR) << "Failed to write oat code to " << out->GetLocation();
    return false;
  }

  // Write padding.
  off_t new_offset = out->Seek(size_executable_offset_alignment_, kSeekCurrent);
  relative_offset += size_executable_offset_alignment_;
  DCHECK_EQ(relative_offset, oat_header_->GetExecutableOffset());
  size_t expected_file_offset = file_offset + relative_offset;
  if (static_cast<uint32_t>(new_offset) != expected_file_offset) {
    PLOG(ERROR) << "Failed to seek to oat code section. Actual: " << new_offset
                << " Expected: " << expected_file_offset << " File: " << out->GetLocation();
    return 0;
  }
  DCHECK_OFFSET();

  write_state_ = WriteState::kWriteText;
  return true;
}

bool OatWriter::WriteCode(OutputStream* out) {
  CHECK(write_state_ == WriteState::kWriteText);

  const size_t file_offset = oat_data_offset_;
  size_t relative_offset = oat_header_->GetExecutableOffset();
  DCHECK_OFFSET();

  relative_offset = WriteCode(out, file_offset, relative_offset);
  if (relative_offset == 0) {
    LOG(ERROR) << "Failed to write oat code to " << out->GetLocation();
    return false;
  }

  relative_offset = WriteCodeDexFiles(out, file_offset, relative_offset);
  if (relative_offset == 0) {
    LOG(ERROR) << "Failed to write oat code for dex files to " << out->GetLocation();
    return false;
  }

  const off_t oat_end_file_offset = out->Seek(0, kSeekCurrent);
  if (oat_end_file_offset == static_cast<off_t>(-1)) {
    LOG(ERROR) << "Failed to get oat end file offset in " << out->GetLocation();
    return false;
  }

  if (kIsDebugBuild) {
    uint32_t size_total = 0;
    #define DO_STAT(x) \
      VLOG(compiler) << #x "=" << PrettySize(x) << " (" << x << "B)"; \
      size_total += x;

    DO_STAT(size_dex_file_alignment_);
    DO_STAT(size_executable_offset_alignment_);
    DO_STAT(size_oat_header_);
    DO_STAT(size_oat_header_key_value_store_);
    DO_STAT(size_dex_file_);
    DO_STAT(size_interpreter_to_interpreter_bridge_);
    DO_STAT(size_interpreter_to_compiled_code_bridge_);
    DO_STAT(size_jni_dlsym_lookup_);
    DO_STAT(size_quick_generic_jni_trampoline_);
    DO_STAT(size_quick_imt_conflict_trampoline_);
    DO_STAT(size_quick_resolution_trampoline_);
    DO_STAT(size_quick_to_interpreter_bridge_);
    DO_STAT(size_trampoline_alignment_);
    DO_STAT(size_method_header_);
    DO_STAT(size_code_);
    DO_STAT(size_code_alignment_);
    DO_STAT(size_relative_call_thunks_);
    DO_STAT(size_misc_thunks_);
    DO_STAT(size_mapping_table_);
    DO_STAT(size_vmap_table_);
    DO_STAT(size_gc_map_);
    DO_STAT(size_oat_dex_file_location_size_);
    DO_STAT(size_oat_dex_file_location_data_);
    DO_STAT(size_oat_dex_file_location_checksum_);
    DO_STAT(size_oat_dex_file_offset_);
    DO_STAT(size_oat_dex_file_class_offsets_offset_);
    DO_STAT(size_oat_dex_file_lookup_table_offset_);
    DO_STAT(size_oat_lookup_table_alignment_);
    DO_STAT(size_oat_lookup_table_);
    DO_STAT(size_oat_class_offsets_alignment_);
    DO_STAT(size_oat_class_offsets_);
    DO_STAT(size_oat_class_type_);
    DO_STAT(size_oat_class_status_);
    DO_STAT(size_oat_class_method_bitmaps_);
    DO_STAT(size_oat_class_method_offsets_);
    #undef DO_STAT

    VLOG(compiler) << "size_total=" << PrettySize(size_total) << " (" << size_total << "B)"; \
    CHECK_EQ(file_offset + size_total, static_cast<size_t>(oat_end_file_offset));
    CHECK_EQ(size_, size_total);
  }

  CHECK_EQ(file_offset + size_, static_cast<size_t>(oat_end_file_offset));
  CHECK_EQ(size_, relative_offset);

  write_state_ = WriteState::kWriteHeader;
  return true;
}

bool OatWriter::WriteHeader(OutputStream* out) {
  CHECK(write_state_ == WriteState::kWriteHeader);
  const size_t file_offset = oat_data_offset_;

  off_t current_offset = out->Seek(0, kSeekCurrent);
  if (current_offset == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to get current offset from " << out->GetLocation();
    return false;
  }
  if (out->Seek(file_offset, kSeekSet) == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to seek to oat header position in " << out->GetLocation();
    return false;
  }
  DCHECK_EQ(file_offset, static_cast<size_t>(out->Seek(0, kSeekCurrent)));

  // Flush all other data before writing the header.
  if (!out->Flush()) {
    PLOG(ERROR) << "Failed to flush before writing oat header to " << out->GetLocation();
    return false;
  }
  // Write the header.
  size_t header_size = oat_header_->GetHeaderSize();
  if (!out->WriteFully(oat_header_.get(), header_size)) {
    PLOG(ERROR) << "Failed to write oat header to " << out->GetLocation();
    return false;
  }
  // Flush the header data.
  if (!out->Flush()) {
    PLOG(ERROR) << "Failed to flush after writing oat header to " << out->GetLocation();
    return false;
  }

  if (out->Seek(current_offset, kSeekSet) == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed to seek back after writing oat header to " << out->GetLocation();
    return false;
  }
  DCHECK_EQ(current_offset, out->Seek(0, kSeekCurrent));

  write_state_ = WriteState::kDone;
  return true;
}

bool OatWriter::WriteClassOffsets(OutputStream* out) {
  for (size_t i = 0; i != oat_dex_files_.size(); ++i) {
    uint32_t expected_offset = oat_data_offset_ + oat_dex_files_[i].class_offsets_offset_;
    off_t actual_offset = out->Seek(expected_offset, kSeekSet);
    if (static_cast<uint32_t>(actual_offset) != expected_offset) {
      const DexFile* dex_file = (*dex_files_)[i];
      PLOG(ERROR) << "Failed to seek to oat class offsets section. Actual: " << actual_offset
                  << " Expected: " << expected_offset << " File: " << dex_file->GetLocation();
      return false;
    }
    if (!WriteData(out,
                   oat_dex_files_[i].class_offsets_.data(),
                   oat_dex_files_[i].GetClassOffsetsRawSize())) {
      PLOG(ERROR) << "Failed to write oat class offsets for " << (*dex_files_)[i]->GetLocation()
                  << " to " << out->GetLocation();
      return false;
    }
    size_oat_class_offsets_ += oat_dex_files_[i].GetClassOffsetsRawSize();
  }
  return true;
}

bool OatWriter::WriteClasses(OutputStream* out) {
  for (OatClass& oat_class : oat_classes_) {
    if (!oat_class.Write(this, out, oat_data_offset_)) {
      PLOG(ERROR) << "Failed to write oat methods information to " << out->GetLocation();
      return false;
    }
  }
  return true;
}

size_t OatWriter::WriteMaps(OutputStream* out, const size_t file_offset, size_t relative_offset) {
  #define VISIT(VisitorType)                                              \
    do {                                                                  \
      VisitorType visitor(this, out, file_offset, relative_offset);       \
      if (UNLIKELY(!VisitDexMethods(&visitor))) {                         \
        return 0;                                                         \
      }                                                                   \
      relative_offset = visitor.GetOffset();                              \
    } while (false)

  size_t gc_maps_offset = relative_offset;
  VISIT(WriteMapMethodVisitor<GcMapDataAccess>);
  size_gc_map_ = relative_offset - gc_maps_offset;

  size_t mapping_tables_offset = relative_offset;
  VISIT(WriteMapMethodVisitor<MappingTableDataAccess>);
  size_mapping_table_ = relative_offset - mapping_tables_offset;

  size_t vmap_tables_offset = relative_offset;
  VISIT(WriteMapMethodVisitor<VmapTableDataAccess>);
  size_vmap_table_ = relative_offset - vmap_tables_offset;

  #undef VISIT

  return relative_offset;
}

size_t OatWriter::WriteCode(OutputStream* out, const size_t file_offset, size_t relative_offset) {
  if (compiler_driver_->IsBootImage()) {
    InstructionSet instruction_set = compiler_driver_->GetInstructionSet();

    #define DO_TRAMPOLINE(field) \
      do { \
        uint32_t aligned_offset = CompiledCode::AlignCode(relative_offset, instruction_set); \
        uint32_t alignment_padding = aligned_offset - relative_offset; \
        out->Seek(alignment_padding, kSeekCurrent); \
        size_trampoline_alignment_ += alignment_padding; \
        if (!WriteData(out, field->data(), field->size())) { \
          PLOG(ERROR) << "Failed to write " # field " to " << out->GetLocation(); \
          return false; \
        } \
        size_ ## field += field->size(); \
        relative_offset += alignment_padding + field->size(); \
        DCHECK_OFFSET(); \
      } while (false)

    DO_TRAMPOLINE(jni_dlsym_lookup_);
    DO_TRAMPOLINE(quick_generic_jni_trampoline_);
    DO_TRAMPOLINE(quick_imt_conflict_trampoline_);
    DO_TRAMPOLINE(quick_resolution_trampoline_);
    DO_TRAMPOLINE(quick_to_interpreter_bridge_);
    #undef DO_TRAMPOLINE
  }
  return relative_offset;
}

size_t OatWriter::WriteCodeDexFiles(OutputStream* out,
                                    const size_t file_offset,
                                    size_t relative_offset) {
  #define VISIT(VisitorType)                                              \
    do {                                                                  \
      VisitorType visitor(this, out, file_offset, relative_offset);       \
      if (UNLIKELY(!VisitDexMethods(&visitor))) {                         \
        return 0;                                                         \
      }                                                                   \
      relative_offset = visitor.GetOffset();                              \
    } while (false)

  VISIT(WriteCodeMethodVisitor);

  #undef VISIT

  size_code_alignment_ += relative_patcher_->CodeAlignmentSize();
  size_relative_call_thunks_ += relative_patcher_->RelativeCallThunksSize();
  size_misc_thunks_ += relative_patcher_->MiscThunksSize();

  return relative_offset;
}

bool OatWriter::GetOatDataOffset(OutputStream* out) {
  // Get the elf file offset of the oat file.
  const off_t raw_file_offset = out->Seek(0, kSeekCurrent);
  if (raw_file_offset == static_cast<off_t>(-1)) {
    LOG(ERROR) << "Failed to get file offset in " << out->GetLocation();
    return false;
  }
  oat_data_offset_ = static_cast<size_t>(raw_file_offset);
  return true;
}

bool OatWriter::ReadDexFileHeader(File* file, OatDexFile* oat_dex_file) {
  // Read the dex file header and perform minimal verification.
  uint8_t raw_header[sizeof(DexFile::Header)];
  if (!file->ReadFully(&raw_header, sizeof(DexFile::Header))) {
    PLOG(ERROR) << "Failed to read dex file header. Actual: "
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (!DexFile::IsMagicValid(raw_header)) {
    LOG(ERROR) << "Invalid magic number in dex file header. "
               << " File: " << oat_dex_file->GetLocation();
    return false;
  }
  if (!DexFile::IsVersionValid(raw_header)) {
    LOG(ERROR) << "Invalid version number in dex file header. "
               << " File: " << oat_dex_file->GetLocation();
    return false;
  }
  typedef DexFile::Header __attribute__((aligned(1))) UnalignedHeader;
  const UnalignedHeader* header = reinterpret_cast<const UnalignedHeader*>(&raw_header);
  if (header->file_size_ < sizeof(DexFile::Header)) {
    LOG(ERROR) << "Dex file header specifies file size insufficient to contain the header."
               << " File: " << oat_dex_file->GetLocation();
    return false;
  }

  oat_dex_file->size_ = header->file_size_;
  oat_dex_file->dex_file_location_checksum_ = header->checksum_;
  oat_dex_file->class_offsets_.resize(header->class_defs_size_);
  return true;
}

bool OatWriter::SeekToDexFile(OutputStream* out, File* file, OatDexFile* oat_dex_file) {
  // Dex files are required to be 4 byte aligned.
  size_t original_offset = size_;
  size_t offset = RoundUp(original_offset, 4);
  size_dex_file_alignment_ += offset - original_offset;

  // Seek to the start of the dex file and flush any pending operations in the stream.
  // Verify that, after flushing the stream, the file is at the same offset as the stream.
  uint32_t start_offset = oat_data_offset_ + offset;
  off_t actual_offset = out->Seek(start_offset, kSeekSet);
  if (actual_offset != static_cast<off_t>(start_offset)) {
    PLOG(ERROR) << "Failed to seek to dex file section. Actual: " << actual_offset
                << " Expected: " << start_offset
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (!out->Flush()) {
    PLOG(ERROR) << "Failed to flush before extracting dex file from ZIP entry."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  actual_offset = lseek(file->Fd(), 0, SEEK_CUR);
  if (actual_offset != static_cast<off_t>(start_offset)) {
    PLOG(ERROR) << "Stream/file position mismatch! Actual: " << actual_offset
                << " Expected: " << start_offset
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }

  size_ = offset;
  oat_dex_file->offset_ = offset;
  return true;
}

bool OatWriter::WriteDexFile(OutputStream* rodata,
                             File* file,
                             OatDexFile* oat_dex_file,
                             ZipEntry* dex_file) {
  size_t start_offset = oat_data_offset_ + size_;
  DCHECK_EQ(static_cast<off_t>(start_offset), rodata->Seek(0, kSeekCurrent));

  // Extract the dex file and get the extracted size.
  std::string error_msg;
  if (!dex_file->ExtractToFile(*file, &error_msg)) {
    LOG(ERROR) << "Failed to extract dex file from ZIP entry: " << error_msg
               << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (!file->Flush()) {
    PLOG(ERROR) << "Failed to flush dex file from ZIP entry."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  off_t extracted_end = lseek(file->Fd(), 0, SEEK_CUR);
  if (extracted_end == static_cast<off_t>(-1)) {
    PLOG(ERROR) << "Failed get end offset after writing dex file from ZIP entry."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (extracted_end < static_cast<off_t>(start_offset)) {
    LOG(ERROR) << "Dex file end position is before start position! End: " << extracted_end
               << " Start: " << start_offset
               << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  uint64_t extracted_size = static_cast<uint64_t>(extracted_end - start_offset);
  if (extracted_size < sizeof(DexFile::Header)) {
    LOG(ERROR) << "Extracted dex file is shorter than dex file header. size: "
               << extracted_size << " File: " << oat_dex_file->GetLocation();
    return false;
  }

  // Read the dex file header and extract required data to OatDexFile.
  off_t actual_offset = lseek(file->Fd(), start_offset, SEEK_SET);
  if (actual_offset != static_cast<off_t>(start_offset)) {
    PLOG(ERROR) << "Failed to seek back to dex file header. Actual: " << actual_offset
                << " Expected: " << start_offset
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (!ReadDexFileHeader(file, oat_dex_file)) {
    return false;
  }
  if (extracted_size < oat_dex_file->size_) {
    LOG(ERROR) << "Extracted truncated dex file. Extracted size: " << extracted_size
               << " file size from header: " << oat_dex_file->size_
               << " File: " << oat_dex_file->GetLocation();
    return false;
  }

  // Override the checksum from header with the CRC from ZIP entry.
  oat_dex_file->dex_file_location_checksum_ = dex_file->GetCrc32();

  // Seek both file and stream to the end offset.
  size_t end_offset = start_offset + oat_dex_file->size_;
  actual_offset = lseek(file->Fd(), end_offset, SEEK_SET);
  if (actual_offset != static_cast<off_t>(end_offset)) {
    PLOG(ERROR) << "Failed to seek to end of dex file. Actual: " << actual_offset
                << " Expected: " << end_offset
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  actual_offset = rodata->Seek(end_offset, kSeekSet);
  if (actual_offset != static_cast<off_t>(end_offset)) {
    PLOG(ERROR) << "Failed to seek stream to end of dex file. Actual: " << actual_offset
                << " Expected: " << end_offset << " File: " << oat_dex_file->GetLocation();
    return false;
  }
  if (!rodata->Flush()) {
    PLOG(ERROR) << "Failed to flush stream after seeking over dex file."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }

  // If we extracted more than the size specified in the header, truncate the file.
  if (extracted_size > oat_dex_file->size_) {
    if (file->SetLength(end_offset) != 0) {
      PLOG(ERROR) << "Failed to truncate excessive dex file length."
                  << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
      return false;
    }
  }

  // Update current size.
  DCHECK_EQ(size_, oat_dex_file->offset_);
  size_ += oat_dex_file->size_;
  return true;
}

bool OatWriter::WriteDexFile(OutputStream* rodata,
                             File* file,
                             OatDexFile* oat_dex_file,
                             File* dex_file) {
  size_t start_offset = oat_data_offset_ + size_;
  DCHECK_EQ(static_cast<off_t>(start_offset), rodata->Seek(0, kSeekCurrent));

  off_t input_offset = lseek(dex_file->Fd(), 0, SEEK_SET);
  if (input_offset != static_cast<off_t>(start_offset)) {
    PLOG(ERROR) << "Failed to seek to dex file header. Actual: " << input_offset
                << " Expected: 0"
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (!ReadDexFileHeader(dex_file, oat_dex_file)) {
    return false;
  }

  // Copy the input dex file using sendfile().
  if (!file->Copy(dex_file, 0, oat_dex_file->size_)) {
    PLOG(ERROR) << "Failed to copy dex file to oat file."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  if (!file->Flush()) {
    PLOG(ERROR) << "Failed to flush dex file."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }

  // Check file position and seek the stream to the end offset.
  size_t end_offset = start_offset + oat_dex_file->size_;
  off_t actual_offset = lseek(file->Fd(), 0, SEEK_CUR);
  if (actual_offset != static_cast<off_t>(end_offset)) {
    PLOG(ERROR) << "Unexpected file position after copying dex file. Actual: " << actual_offset
                << " Expected: " << end_offset
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }
  actual_offset = rodata->Seek(end_offset, kSeekSet);
  if (actual_offset != static_cast<off_t>(end_offset)) {
    PLOG(ERROR) << "Failed to seek stream to end of dex file. Actual: " << actual_offset
                << " Expected: " << end_offset << " File: " << oat_dex_file->GetLocation();
    return false;
  }
  if (!rodata->Flush()) {
    PLOG(ERROR) << "Failed to flush stream after seeking over dex file."
                << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
    return false;
  }

  // Update current size.
  DCHECK_EQ(size_, oat_dex_file->offset_);
  size_ += oat_dex_file->size_;

  static constexpr bool kCheckCopy = true;
  if (kCheckCopy) {
    actual_offset = lseek(file->Fd(), start_offset, SEEK_SET);
    if (actual_offset != static_cast<off_t>(start_offset)) {
      PLOG(ERROR) << "CheckCopy: Failed to seek to dex file header. Actual: " << actual_offset
                  << " Expected: " << start_offset
                  << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
      return false;
    }

    // Read the dex file header and compare with original values.
    OatDexFile check_oat_dex_file(oat_dex_file->offset_, oat_dex_file->GetLocation());
    if (!ReadDexFileHeader(file, &check_oat_dex_file)) {
      PLOG(ERROR) << "CheckCopy: Failed to seek to read dex file header.";
      return false;
    }

    if (oat_dex_file->size_ != check_oat_dex_file.size_) {
      LOG(ERROR) << "CheckCopy: size mismatch: "
          << oat_dex_file->size_ << "!=" << check_oat_dex_file.size_;
      return false;
    }
    if (oat_dex_file->dex_file_location_checksum_ !=
        check_oat_dex_file.dex_file_location_checksum_) {
      LOG(ERROR) << "CheckCopy: checksum mismatch: "
          << oat_dex_file->dex_file_location_checksum_ << "!="
          << check_oat_dex_file.dex_file_location_checksum_;
      return false;
    }
    if (oat_dex_file->class_offsets_.size() != check_oat_dex_file.class_offsets_.size()) {
      LOG(ERROR) << "CheckCopy: number of class definitions mismatch: "
          << oat_dex_file->class_offsets_.size() << "!="
          << check_oat_dex_file.class_offsets_.size();
      return false;
    }

    actual_offset = lseek(file->Fd(), end_offset, SEEK_SET);
    if (actual_offset != static_cast<off_t>(end_offset)) {
      PLOG(ERROR) << "CheckCopy: Failed to seek to dex file header. Actual: " << actual_offset
                  << " Expected: " << end_offset
                  << " File: " << oat_dex_file->GetLocation() << " Output: " << file->GetPath();
      return false;
    }
  }

  return true;
}

bool OatWriter::WriteCodeAlignment(OutputStream* out, uint32_t aligned_code_delta) {
  static const uint8_t kPadding[] = {
      0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
  };
  DCHECK_LE(aligned_code_delta, sizeof(kPadding));
  if (UNLIKELY(!out->WriteFully(kPadding, aligned_code_delta))) {
    return false;
  }
  size_code_alignment_ += aligned_code_delta;
  return true;
}

bool OatWriter::WriteData(OutputStream* out, const void* data, size_t size) {
  oat_header_->UpdateChecksum(data, size);
  return out->WriteFully(data, size);
}

std::pair<bool, uint32_t> OatWriter::MethodOffsetMap::FindMethodOffset(MethodReference ref) {
  auto it = map.find(ref);
  if (it == map.end()) {
    return std::pair<bool, uint32_t>(false, 0u);
  } else {
    return std::pair<bool, uint32_t>(true, it->second);
  }
}

OatWriter::OatDexFile::OatDexFile(size_t offset, const char* dex_file_location)
    : offset_(offset),
      size_(0),
      dex_file_location_size_(strlen(dex_file_location)),
      dex_file_location_data_(reinterpret_cast<const uint8_t*>(dex_file_location)),
      dex_file_location_checksum_(0u),
      dex_file_offset_(0u),
      class_offsets_offset_(0u),
      lookup_table_offset_(0u),
      class_offsets_() {
}

size_t OatWriter::OatDexFile::SizeOf() const {
  return sizeof(dex_file_location_size_)
          + dex_file_location_size_
          + sizeof(dex_file_location_checksum_)
          + sizeof(dex_file_offset_)
          + sizeof(class_offsets_offset_)
          + sizeof(lookup_table_offset_);
}

bool OatWriter::OatDexFile::Write(OatWriter* oat_writer,
                                  OutputStream* out,
                                  const DexFile& dex_file) const {
  const size_t file_offset = oat_writer->oat_data_offset_;
  DCHECK_OFFSET_();

  if (!oat_writer->WriteData(out, &dex_file_location_size_, sizeof(dex_file_location_size_))) {
    PLOG(ERROR) << "Failed to write dex file location length to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_location_size_ += sizeof(dex_file_location_size_);

  CHECK_EQ(dex_file.GetLocation().size(), dex_file_location_size_);
  CHECK_EQ(memcmp(dex_file.GetLocation().data(), dex_file_location_data_, dex_file_location_size_),
           0);
  if (!oat_writer->WriteData(out, dex_file_location_data_, dex_file_location_size_)) {
    PLOG(ERROR) << "Failed to write dex file location data to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_location_data_ += dex_file_location_size_;

  if (!oat_writer->WriteData(out, &dex_file_location_checksum_, sizeof(dex_file_location_checksum_))) {
    PLOG(ERROR) << "Failed to write dex file location checksum to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_location_checksum_ += sizeof(dex_file_location_checksum_);

  if (!oat_writer->WriteData(out, &dex_file_offset_, sizeof(dex_file_offset_))) {
    PLOG(ERROR) << "Failed to write dex file offset to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_offset_ += sizeof(dex_file_offset_);

  if (!oat_writer->WriteData(out, &class_offsets_offset_, sizeof(class_offsets_offset_))) {
    PLOG(ERROR) << "Failed to write class offsets offset to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_class_offsets_offset_ += sizeof(class_offsets_offset_);

  if (!oat_writer->WriteData(out, &lookup_table_offset_, sizeof(lookup_table_offset_))) {
    PLOG(ERROR) << "Failed to write lookup table offset to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_dex_file_lookup_table_offset_ += sizeof(lookup_table_offset_);

  return true;
}

OatWriter::OatClass::OatClass(size_t offset,
                              const dchecked_vector<CompiledMethod*>& compiled_methods,
                              uint32_t num_non_null_compiled_methods,
                              mirror::Class::Status status)
    : compiled_methods_(compiled_methods) {
  uint32_t num_methods = compiled_methods.size();
  CHECK_LE(num_non_null_compiled_methods, num_methods);

  offset_ = offset;
  oat_method_offsets_offsets_from_oat_class_.resize(num_methods);

  // Since both kOatClassNoneCompiled and kOatClassAllCompiled could
  // apply when there are 0 methods, we just arbitrarily say that 0
  // methods means kOatClassNoneCompiled and that we won't use
  // kOatClassAllCompiled unless there is at least one compiled
  // method. This means in an interpretter only system, we can assert
  // that all classes are kOatClassNoneCompiled.
  if (num_non_null_compiled_methods == 0) {
    type_ = kOatClassNoneCompiled;
  } else if (num_non_null_compiled_methods == num_methods) {
    type_ = kOatClassAllCompiled;
  } else {
    type_ = kOatClassSomeCompiled;
  }

  status_ = status;
  method_offsets_.resize(num_non_null_compiled_methods);
  method_headers_.resize(num_non_null_compiled_methods);

  uint32_t oat_method_offsets_offset_from_oat_class = sizeof(type_) + sizeof(status_);
  if (type_ == kOatClassSomeCompiled) {
    method_bitmap_.reset(new BitVector(num_methods, false, Allocator::GetMallocAllocator()));
    method_bitmap_size_ = method_bitmap_->GetSizeOf();
    oat_method_offsets_offset_from_oat_class += sizeof(method_bitmap_size_);
    oat_method_offsets_offset_from_oat_class += method_bitmap_size_;
  } else {
    method_bitmap_ = nullptr;
    method_bitmap_size_ = 0;
  }

  for (size_t i = 0; i < num_methods; i++) {
    CompiledMethod* compiled_method = compiled_methods_[i];
    if (compiled_method == nullptr) {
      oat_method_offsets_offsets_from_oat_class_[i] = 0;
    } else {
      oat_method_offsets_offsets_from_oat_class_[i] = oat_method_offsets_offset_from_oat_class;
      oat_method_offsets_offset_from_oat_class += sizeof(OatMethodOffsets);
      if (type_ == kOatClassSomeCompiled) {
        method_bitmap_->SetBit(i);
      }
    }
  }
}

size_t OatWriter::OatClass::GetOatMethodOffsetsOffsetFromOatHeader(
    size_t class_def_method_index_) const {
  uint32_t method_offset = GetOatMethodOffsetsOffsetFromOatClass(class_def_method_index_);
  if (method_offset == 0) {
    return 0;
  }
  return offset_ + method_offset;
}

size_t OatWriter::OatClass::GetOatMethodOffsetsOffsetFromOatClass(
    size_t class_def_method_index_) const {
  return oat_method_offsets_offsets_from_oat_class_[class_def_method_index_];
}

size_t OatWriter::OatClass::SizeOf() const {
  return sizeof(status_)
          + sizeof(type_)
          + ((method_bitmap_size_ == 0) ? 0 : sizeof(method_bitmap_size_))
          + method_bitmap_size_
          + (sizeof(method_offsets_[0]) * method_offsets_.size());
}

bool OatWriter::OatClass::Write(OatWriter* oat_writer,
                                OutputStream* out,
                                const size_t file_offset) const {
  DCHECK_OFFSET_();
  if (!oat_writer->WriteData(out, &status_, sizeof(status_))) {
    PLOG(ERROR) << "Failed to write class status to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_class_status_ += sizeof(status_);

  if (!oat_writer->WriteData(out, &type_, sizeof(type_))) {
    PLOG(ERROR) << "Failed to write oat class type to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_class_type_ += sizeof(type_);

  if (method_bitmap_size_ != 0) {
    CHECK_EQ(kOatClassSomeCompiled, type_);
    if (!oat_writer->WriteData(out, &method_bitmap_size_, sizeof(method_bitmap_size_))) {
      PLOG(ERROR) << "Failed to write method bitmap size to " << out->GetLocation();
      return false;
    }
    oat_writer->size_oat_class_method_bitmaps_ += sizeof(method_bitmap_size_);

    if (!oat_writer->WriteData(out, method_bitmap_->GetRawStorage(), method_bitmap_size_)) {
      PLOG(ERROR) << "Failed to write method bitmap to " << out->GetLocation();
      return false;
    }
    oat_writer->size_oat_class_method_bitmaps_ += method_bitmap_size_;
  }

  if (!oat_writer->WriteData(out,
                             method_offsets_.data(),
                             sizeof(method_offsets_[0]) * method_offsets_.size())) {
    PLOG(ERROR) << "Failed to write method offsets to " << out->GetLocation();
    return false;
  }
  oat_writer->size_oat_class_method_offsets_ += sizeof(method_offsets_[0]) * method_offsets_.size();
  return true;
}

}  // namespace art

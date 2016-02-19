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

#include "image.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/unix_file/fd_file.h"
#include "class_linker-inl.h"
#include "common_compiler_test.h"
#include "debug/method_debug_info.h"
#include "driver/compiler_options.h"
#include "elf_writer.h"
#include "elf_writer_quick.h"
#include "gc/space/image_space.h"
#include "image_writer.h"
#include "lock_word.h"
#include "mirror/object-inl.h"
#include "oat_writer.h"
#include "scoped_thread_state_change.h"
#include "signal_catcher.h"
#include "utils.h"

namespace art {

class ImageTest : public CommonCompilerTest {
 protected:
  void SetUp() {
    ReserveImageSpace();
    CommonCompilerTest::SetUp();
  }
  void TearDown() {
    UnreserveImageSpace();
    CommonCompilerTest::TearDown();
  }

  // The following two functions set up an environment that is identical to dex2oat
  // --force-determinism.
  void SetUpRuntimeOptions(RuntimeOptions* raw_options) {
    CommonCompilerTest::SetUpRuntimeOptions(raw_options);

    raw_options->push_back(std::make_pair("-Xgc:nonconcurrent", nullptr));
    raw_options->push_back(std::make_pair("-XX:LargeObjectSpace=freelist", nullptr));

    // We also need to turn off the nonmoving space. For that, we need to disable HSpace
    // compaction (done above) and ensure that neither foreground nor background collectors
    // are concurrent.
    raw_options->push_back(std::make_pair("-XX:BackgroundGC=nonconcurrent", nullptr));
  }

  void PreRuntimeCreate() {
    // To make identity hashcode deterministic, set a known seed.
    mirror::Object::SetHashCodeSeed(987654321U);
  }

  size_t HashWriteRead();
  void TestWriteRead(ImageHeader::StorageMode storage_mode);

  std::unique_ptr<ImageWriter> writer_;

//  raw_options.push_back(std::make_pair("-Xgc:nonconcurrent", nullptr));
//        raw_options.push_back(std::make_pair("-XX:LargeObjectSpace=freelist", nullptr));
//
//        // We also need to turn off the nonmoving space. For that, we need to disable HSpace
//        // compaction (done above) and ensure that neither foreground nor background collectors
//        // are concurrent.
//        raw_options.push_back(std::make_pair("-XX:BackgroundGC=nonconcurrent", nullptr));
//
//        // To make identity hashcode deterministic, set a known seed.
//        mirror::Object::SetHashCodeSeed(987654321U);
 private:
  using TestFn = std::function<void (const std::string& image_filename,  // NOLINT [whitespace/parens] [4]
                                     const std::string& oat_filename,
                                     const std::string& image_location,
                                     const uintptr_t requested_image_base)>;
  void TestFramework(bool set_fixup,
                     ImageHeader::StorageMode storage_mode,
                     TestFn tester);
};

void ImageTest::TestFramework(bool set_fixup,
                              ImageHeader::StorageMode storage_mode,
                              TestFn tester) {
  CreateCompilerDriver(Compiler::kOptimizing, kRuntimeISA, 16);

  // Set inline filter values.
  compiler_options_->SetInlineDepthLimit(CompilerOptions::kDefaultInlineDepthLimit);
  compiler_options_->SetInlineMaxCodeUnits(CompilerOptions::kDefaultInlineMaxCodeUnits);
  compiler_options_->SetForceDeterminism(true);

  if (set_fixup) {
    compiler_driver_->SetSupportBootImageFixup(true);
  }
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  // Enable write for dex2dex.
  for (const DexFile* dex_file : class_linker->GetBootClassPath()) {
    dex_file->EnableWrite();
  }
  // Create a generic location tmp file, to be the base of the .art and .oat temporary files.
  std::string image_location = std::string(getenv("ANDROID_DATA")) + "/image-test.art";

  std::string image_filename(GetSystemImageFilename(image_location.c_str(), kRuntimeISA));
  size_t pos = image_filename.rfind('/');
  CHECK_NE(pos, std::string::npos) << image_filename;
  std::string image_dir(image_filename, 0, pos);
  int mkdir_result = mkdir(image_dir.c_str(), 0700);
  CHECK_EQ(0, mkdir_result) << image_dir;
  ScratchFile image_file(OS::CreateEmptyFile(image_filename.c_str()));

  std::string oat_filename(image_filename, 0, image_filename.size() - 3);
  oat_filename += "oat";
  ScratchFile oat_file(OS::CreateEmptyFile(oat_filename.c_str()));

  const uintptr_t requested_image_base = ART_BASE_ADDRESS;
  std::unordered_map<const DexFile*, const char*> dex_file_to_oat_filename_map;
  std::vector<const char*> oat_filename_vector(1, oat_filename.c_str());
  for (const DexFile* dex_file : class_linker->GetBootClassPath()) {
    dex_file_to_oat_filename_map.emplace(dex_file, oat_filename.c_str());
  }
  writer_.reset(new ImageWriter(*compiler_driver_,
                                requested_image_base,
                                /*compile_pic*/false,
                                /*compile_app_image*/false,
                                storage_mode,
                                oat_filename_vector,
                                dex_file_to_oat_filename_map));
  // TODO: compile_pic should be a test argument.
  {
    {
      jobject class_loader = nullptr;
      TimingLogger timings("ImageTest::WriteRead", false, false);
      TimingLogger::ScopedTiming t("CompileAll", &timings);
      compiler_driver_->SetDexFilesForOatFile(class_linker->GetBootClassPath());
      compiler_driver_->CompileAll(class_loader, class_linker->GetBootClassPath(), &timings);

      t.NewTiming("WriteElf");
      SafeMap<std::string, std::string> key_value_store;
      const std::vector<const DexFile*>& dex_files = class_linker->GetBootClassPath();
      std::unique_ptr<ElfWriter> elf_writer = CreateElfWriterQuick(
          compiler_driver_->GetInstructionSet(),
          &compiler_driver_->GetCompilerOptions(),
          oat_file.GetFile());
      elf_writer->Start();
      OatWriter oat_writer(/*compiling_boot_image*/true, &timings);
      OutputStream* rodata = elf_writer->StartRoData();
      for (const DexFile* dex_file : dex_files) {
        ArrayRef<const uint8_t> raw_dex_file(
            reinterpret_cast<const uint8_t*>(&dex_file->GetHeader()),
            dex_file->GetHeader().file_size_);
        oat_writer.AddRawDexFileSource(raw_dex_file,
                                       dex_file->GetLocation().c_str(),
                                       dex_file->GetLocationChecksum());
      }
      std::unique_ptr<MemMap> opened_dex_files_map;
      std::vector<std::unique_ptr<const DexFile>> opened_dex_files;
      bool dex_files_ok = oat_writer.WriteAndOpenDexFiles(
          rodata,
          oat_file.GetFile(),
          compiler_driver_->GetInstructionSet(),
          compiler_driver_->GetInstructionSetFeatures(),
          &key_value_store,
          /* verify */ false,           // Dex files may be dex-to-dex-ed, don't verify.
          &opened_dex_files_map,
          &opened_dex_files);
      ASSERT_TRUE(dex_files_ok);
      oat_writer.PrepareLayout(compiler_driver_.get(), writer_.get(), dex_files);
      bool image_space_ok = writer_->PrepareImageAddressSpace();
      ASSERT_TRUE(image_space_ok);

      bool rodata_ok = oat_writer.WriteRodata(rodata);
      ASSERT_TRUE(rodata_ok);
      elf_writer->EndRoData(rodata);

      OutputStream* text = elf_writer->StartText();
      bool text_ok = oat_writer.WriteCode(text);
      ASSERT_TRUE(text_ok);
      elf_writer->EndText(text);

      bool header_ok = oat_writer.WriteHeader(elf_writer->GetStream(), 0u, 0u, 0u);
      ASSERT_TRUE(header_ok);

      elf_writer->SetBssSize(oat_writer.GetBssSize());
      elf_writer->WriteDynamicSection();
      elf_writer->WriteDebugInfo(oat_writer.GetMethodDebugInfo());
      elf_writer->WritePatchLocations(oat_writer.GetAbsolutePatchLocations());

      bool success = elf_writer->End();

      ASSERT_TRUE(success);
    }
  }
  // Workound bug that mcld::Linker::emit closes oat_file by reopening as dup_oat.
  std::unique_ptr<File> dup_oat(OS::OpenFileReadWrite(oat_file.GetFilename().c_str()));
  ASSERT_TRUE(dup_oat.get() != nullptr);

  {
    std::vector<const char*> dup_oat_filename(1, dup_oat->GetPath().c_str());
    std::vector<const char*> dup_image_filename(1, image_file.GetFilename().c_str());
    bool success_image = writer_->Write(kInvalidFd,
                                       dup_image_filename,
                                       kInvalidFd,
                                       dup_oat_filename,
                                       dup_oat_filename[0]);
    ASSERT_TRUE(success_image);
    bool success_fixup = ElfWriter::Fixup(dup_oat.get(),
                                          writer_->GetOatDataBegin(dup_oat_filename[0]));
    ASSERT_TRUE(success_fixup);

    ASSERT_EQ(dup_oat->FlushCloseOrErase(), 0) << "Could not flush and close oat file "
                                               << oat_file.GetFilename();
  }

  // TODO: Lambda...
  tester(image_filename, oat_filename, image_location, requested_image_base);

  // Cleanup.
  writer_.reset();
  image_file.Unlink();
  oat_file.Unlink();
  int rmdir_result = rmdir(image_dir.c_str());
  CHECK_EQ(0, rmdir_result);
}

// Use murmur3 hash as used in dedupe.
template <typename ContentType>
class HashFunc {
 private:
  static constexpr bool kUseMurmur3Hash = true;

 public:
  size_t operator()(const ArrayRef<ContentType>& array) const {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(array.data());
    // TODO: More reasonable assertion.
    // static_assert(IsPowerOfTwo(sizeof(ContentType)),
    //    "ContentType is not power of two, don't know whether array layout is as assumed");
    uint32_t len = sizeof(ContentType) * array.size();
    if (kUseMurmur3Hash) {
      static constexpr uint32_t c1 = 0xcc9e2d51;
      static constexpr uint32_t c2 = 0x1b873593;
      static constexpr uint32_t r1 = 15;
      static constexpr uint32_t r2 = 13;
      static constexpr uint32_t m = 5;
      static constexpr uint32_t n = 0xe6546b64;

      uint32_t hash = 0;

      const int nblocks = len / 4;
      typedef __attribute__((__aligned__(1))) uint32_t unaligned_uint32_t;
      const unaligned_uint32_t *blocks = reinterpret_cast<const uint32_t*>(data);
      int i;
      for (i = 0; i < nblocks; i++) {
        uint32_t k = blocks[i];
        k *= c1;
        k = (k << r1) | (k >> (32 - r1));
        k *= c2;

        hash ^= k;
        hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
      }

      const uint8_t *tail = reinterpret_cast<const uint8_t*>(data + nblocks * 4);
      uint32_t k1 = 0;

      switch (len & 3) {
        case 3:
          k1 ^= tail[2] << 16;
          FALLTHROUGH_INTENDED;
        case 2:
          k1 ^= tail[1] << 8;
          FALLTHROUGH_INTENDED;
        case 1:
          k1 ^= tail[0];

          k1 *= c1;
          k1 = (k1 << r1) | (k1 >> (32 - r1));
          k1 *= c2;
          hash ^= k1;
      }

      hash ^= len;
      hash ^= (hash >> 16);
      hash *= 0x85ebca6b;
      hash ^= (hash >> 13);
      hash *= 0xc2b2ae35;
      hash ^= (hash >> 16);

      return hash;
    } else {
      size_t hash = 0x811c9dc5;
      for (uint32_t i = 0; i < len; ++i) {
        hash = (hash * 16777619) ^ data[i];
      }
      hash += hash << 13;
      hash ^= hash >> 7;
      hash += hash << 3;
      hash ^= hash >> 17;
      hash += hash << 5;
      return hash;
    }
  }
};

size_t ImageTest::HashWriteRead() {
  size_t hash = 0;
  static size_t count = 0;
  TestFramework(kRuntimeISA != kMips && kRuntimeISA != kMips64,
                ImageHeader::StorageMode::kStorageModeUncompressed,
                [&] (const std::string& image_filename,
                     const std::string& oat_filename,
                     const std::string& image_location ATTRIBUTE_UNUSED,
                     const uintptr_t requested_image_base ATTRIBUTE_UNUSED) {
    uint32_t hashes[2] = { 0, 0 };
    for (size_t i = 0; i < 2; ++i) {
      const char* filename = (i == 0) ? image_filename.c_str() : oat_filename.c_str();
      // TODO: Multi-image?
      std::unique_ptr<File> file(OS::OpenFileForReading(filename));
      ASSERT_FALSE(file == nullptr);

      int64_t file_size = file->GetLength();
      std::unique_ptr<uint8_t[]> buffer(new uint8_t[file_size]);
      ASSERT_TRUE(file->ReadFully(buffer.get(), static_cast<size_t>(file_size)));

      size_t computed_hash = HashFunc<uint8_t>()(ArrayRef<uint8_t>(buffer.get(),
                                                                   static_cast<size_t>(file_size)));
      // Murmur3 hash is really 32-bit, so we don't lose data here.
      hashes[i] = static_cast<uint32_t>(computed_hash);
    }
    if (sizeof(size_t) == 4) {
      // Merge them together.
      hash = hashes[0] ^ hashes[1];
    } else {
      // Use them side-by-side.
      hash = static_cast<size_t>(hashes[0] | (static_cast<uint64_t>(hashes[1]) << 32));
    }

    // TODO: copy the output...
  });
  count++;
  return hash;
}

void ImageTest::TestWriteRead(ImageHeader::StorageMode storage_mode) {
  TestFramework(kRuntimeISA != kMips && kRuntimeISA != kMips64,
                storage_mode,
                [&] (const std::string& image_filename,
                     const std::string& oat_filename ATTRIBUTE_UNUSED,
                     const std::string& image_location,
                     const uintptr_t requested_image_base) {
    uint64_t image_file_size;
    {
      std::unique_ptr<File> file(OS::OpenFileForReading(image_filename.c_str()));
      ASSERT_TRUE(file.get() != nullptr);
      ImageHeader image_header;
      ASSERT_EQ(file->ReadFully(&image_header, sizeof(image_header)), true);
      ASSERT_TRUE(image_header.IsValid());
      const auto& bitmap_section = image_header.GetImageSection(ImageHeader::kSectionImageBitmap);
      ASSERT_GE(bitmap_section.Offset(), sizeof(image_header));
      ASSERT_NE(0U, bitmap_section.Size());

      gc::Heap* heap = Runtime::Current()->GetHeap();
      ASSERT_TRUE(heap->HaveContinuousSpaces());
      gc::space::ContinuousSpace* space = heap->GetNonMovingSpace();
      ASSERT_FALSE(space->IsImageSpace());
      ASSERT_TRUE(space != nullptr);
      ASSERT_TRUE(space->IsMallocSpace());

      image_file_size = file->GetLength();
    }

    ASSERT_TRUE(compiler_driver_->GetImageClasses() != nullptr);
    std::unordered_set<std::string> image_classes(*compiler_driver_->GetImageClasses());

    // Need to delete the compiler since it has worker threads which are attached to runtime.
    compiler_driver_.reset();

    // Tear down old runtime before making a new one, clearing out misc state.

    // Remove the reservation of the memory for use to load the image.
    // Need to do this before we reset the runtime.
    UnreserveImageSpace();
    writer_.reset(nullptr);

    runtime_.reset();
    java_lang_dex_file_ = nullptr;

    MemMap::Init();
    std::unique_ptr<const DexFile> dex(LoadExpectSingleDexFile(GetLibCoreDexFileNames()[0].c_str()));

    RuntimeOptions options;
    std::string image("-Ximage:");
    image.append(image_location);
    options.push_back(std::make_pair(image.c_str(), static_cast<void*>(nullptr)));
    // By default the compiler this creates will not include patch information.
    options.push_back(std::make_pair("-Xnorelocate", nullptr));

    if (!Runtime::Create(options, false)) {
      LOG(FATAL) << "Failed to create runtime";
      return;
    }
    runtime_.reset(Runtime::Current());
    // Runtime::Create acquired the mutator_lock_ that is normally given away when we Runtime::Start,
    // give it away now and then switch to a more managable ScopedObjectAccess.
    Thread::Current()->TransitionFromRunnableToSuspended(kNative);
    ScopedObjectAccess soa(Thread::Current());
    ASSERT_TRUE(runtime_.get() != nullptr);
    class_linker_ = runtime_->GetClassLinker();

    gc::Heap* heap = Runtime::Current()->GetHeap();
    ASSERT_TRUE(heap->HasBootImageSpace());
    ASSERT_TRUE(heap->GetNonMovingSpace()->IsMallocSpace());

    // We loaded the runtime with an explicit image, so it must exist.
    gc::space::ImageSpace* image_space = heap->GetBootImageSpaces()[0];
    ASSERT_TRUE(image_space != nullptr);
    if (storage_mode == ImageHeader::kStorageModeUncompressed) {
      // Uncompressed, image should be smaller than file.
      ASSERT_LE(image_space->Size(), image_file_size);
    } else {
      // Compressed, file should be smaller than image.
      ASSERT_LE(image_file_size, image_space->Size());
    }

    image_space->VerifyImageAllocations();
    uint8_t* image_begin = image_space->Begin();
    uint8_t* image_end = image_space->End();
    CHECK_EQ(requested_image_base, reinterpret_cast<uintptr_t>(image_begin));
    for (size_t i = 0; i < dex->NumClassDefs(); ++i) {
      const DexFile::ClassDef& class_def = dex->GetClassDef(i);
      const char* descriptor = dex->GetClassDescriptor(class_def);
      mirror::Class* klass = class_linker_->FindSystemClass(soa.Self(), descriptor);
      EXPECT_TRUE(klass != nullptr) << descriptor;
      if (image_classes.find(descriptor) != image_classes.end()) {
        // Image classes should be located inside the image.
        EXPECT_LT(image_begin, reinterpret_cast<uint8_t*>(klass)) << descriptor;
        EXPECT_LT(reinterpret_cast<uint8_t*>(klass), image_end) << descriptor;
      } else {
        EXPECT_TRUE(reinterpret_cast<uint8_t*>(klass) >= image_end ||
                    reinterpret_cast<uint8_t*>(klass) < image_begin) << descriptor;
      }
      EXPECT_TRUE(Monitor::IsValidLockWord(klass->GetLockWord(false)));
    }
  });
}

TEST_F(ImageTest, WriteReadUncompressed) {
  TestWriteRead(ImageHeader::kStorageModeUncompressed);
}

TEST_F(ImageTest, WriteReadLZ4) {
  TestWriteRead(ImageHeader::kStorageModeLZ4);
}

TEST_F(ImageTest, ImageHeaderIsValid) {
    uint32_t image_begin = ART_BASE_ADDRESS;
    uint32_t image_size_ = 16 * KB;
    uint32_t image_roots = ART_BASE_ADDRESS + (1 * KB);
    uint32_t oat_checksum = 0;
    uint32_t oat_file_begin = ART_BASE_ADDRESS + (4 * KB);  // page aligned
    uint32_t oat_data_begin = ART_BASE_ADDRESS + (8 * KB);  // page aligned
    uint32_t oat_data_end = ART_BASE_ADDRESS + (9 * KB);
    uint32_t oat_file_end = ART_BASE_ADDRESS + (10 * KB);
    ImageSection sections[ImageHeader::kSectionCount];
    ImageHeader image_header(image_begin,
                             image_size_,
                             sections,
                             image_roots,
                             oat_checksum,
                             oat_file_begin,
                             oat_data_begin,
                             oat_data_end,
                             oat_file_end,
                             /*boot_image_begin*/0U,
                             /*boot_image_size*/0U,
                             /*boot_oat_begin*/0U,
                             /*boot_oat_size_*/0U,
                             sizeof(void*),
                             /*compile_pic*/false,
                             /*is_pic*/false,
                             ImageHeader::kDefaultStorageMode,
                             /*data_size*/0u);
    ASSERT_TRUE(image_header.IsValid());
    ASSERT_TRUE(!image_header.IsAppImage());

    char* magic = const_cast<char*>(image_header.GetMagic());
    strcpy(magic, "");  // bad magic
    ASSERT_FALSE(image_header.IsValid());
    strcpy(magic, "art\n000");  // bad version
    ASSERT_FALSE(image_header.IsValid());
}

TEST_F(ImageTest, Determinism) {
  if (!kIsTargetBuild) {
    uint64_t start = NanoTime();
    // Only do this on the host. We don't care about determinism on the target.
    const size_t base_hash = HashWriteRead();

    constexpr size_t kRounds = 10;
    for (size_t i = 1; i < kRounds; ++i) {
      // Shutdown and restart the runtime. Otherwise dex files might have been quickened. This is
      // somewhat dirty.
      TearDown();
      runtime_.reset();

      SetUp();

      size_t new_hash = HashWriteRead();
      EXPECT_EQ(base_hash, new_hash);
    }

    uint64_t end = NanoTime();
    LOG(ERROR) << "Took " << (end - start)/1000000000.0 << " seconds";
  }
}

}  // namespace art

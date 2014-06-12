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
#ifndef ART_BASE_ADDRESS_MAX_DELTA
#define ART_BASE_ADDRESS_MAX_DELTA 0xf000000
#endif

#include "patchoat.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <random>
#include <vector>

#include "base/stringpiece.h"
#include "base/stringprintf.h"
#include "elf_utils.h"
#include "elf_file.h"
#include "image.h"
#include "mirror/art_field.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method.h"
#include "mirror/art_method-inl.h"
#include "mirror/object.h"
#include "mirror/object-inl.h"
#include "mirror/reference.h"
#include "noop_compiler_callbacks.h"
#include "offsets.h"
#include "os.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "thread.h"
#include "utils.h"

namespace art {

static const char* ElfISAToRuntimeString(Elf32_Word isa) {
  switch (isa) {
    case EM_ARM:
      return "arm";
    case EM_AARCH64:
      return "arm64";
    case EM_386:
      return "x86";
    case EM_X86_64:
      return "x86_64";
    case EM_MIPS:
      return "mips";
    default:
      return nullptr;
  }
}

bool PatchOat::Patch(const File* input_oat, const std::string& art_location, off_t delta,
                     File* output_oat, File* output_art, TimingLogger& timings) {
  std::string err;
  CHECK(Runtime::Current() == nullptr);
  CHECK(output_art != nullptr);
  CHECK_GE(output_art->Fd(), 0);
  CHECK(input_oat != nullptr);
  CHECK(output_oat != nullptr);
  CHECK_GE(input_oat->Fd(), 0);
  CHECK_GE(output_oat->Fd(), 0);
  CHECK(!art_location.empty()) << "art file must have a filename.";

  TimingLogger::ScopedTiming t("Runtime Setup", &timings);

  Elf32_Ehdr elf_hdr;
  if (sizeof(elf_hdr) != input_oat->Read(reinterpret_cast<char*>(&elf_hdr), sizeof(elf_hdr), 0)) {
    LOG(ERROR) << "unable to read elf header";
    return false;
  }
  const char* isa_name = ElfISAToRuntimeString(elf_hdr.e_machine);
  std::string image_filename(GetSystemImageFilename(art_location.c_str(),
                                                    GetInstructionSetFromString(isa_name)));
  std::unique_ptr<File> input_art(OS::OpenFileForReading(image_filename.c_str()));
  if (input_art.get() == nullptr) {
    LOG(ERROR) << "unable to open input art file.";
    return false;
  }
  int64_t art_len = input_art->GetLength();
  if (art_len < 0) {
    LOG(ERROR) << "Error while getting image length";
    return false;
  }
  ImageHeader image_header;
  if (sizeof(image_header) != input_art->Read(reinterpret_cast<char*>(&image_header),
                                              sizeof(image_header), 0)) {
    LOG(ERROR) << "Unable to read image header from image file " << input_art->GetPath();
  }

  // Set up the runtime
  Runtime::Options options;
  NoopCompilerCallbacks callbacks;
  options.push_back(std::make_pair("compilercallbacks", &callbacks));
  std::string img = "-Ximage:"+art_location;
  options.push_back(std::make_pair(img.c_str(), nullptr));
  options.push_back(std::make_pair("imageinstructionset", reinterpret_cast<const void*>(isa_name)));
  if (!Runtime::Create(options, false)) {
    LOG(ERROR) << "Unable to initialize runtime";
    return false;
  }
  // Runtime::Create acquired the mutator_lock_ that is normally given away when we Runtime::Start,
  // give it away now and then switch to a more manageable ScopedObjectAccess.
  Thread::Current()->TransitionFromRunnableToSuspended(kNative);
  ScopedObjectAccess soa(Thread::Current());

  t.NewTiming("Image and oat Patching setup");
  // Create the map where we will write the image patches to.
  std::unique_ptr<MemMap> image(MemMap::MapFile(art_len, PROT_READ | PROT_WRITE, MAP_PRIVATE,
                                                input_art->Fd(), 0, input_art->GetPath().c_str(),
                                                &err));
  if (image.get() == nullptr) {
    LOG(ERROR) << "unable to map art file " << input_art->GetPath() << " : " << err;
    return false;
  }
  gc::space::ImageSpace* ispc = Runtime::Current()->GetHeap()->GetImageSpace();

  std::unique_ptr<ElfFile> elf(ElfFile::Open(const_cast<File*>(input_oat),
                                             PROT_READ | PROT_WRITE, MAP_PRIVATE, &err));
  if (elf.get() == nullptr) {
    LOG(ERROR) << "unable to open oat file " << input_oat->GetPath() << " : " << err;
    return false;
  }

  PatchOat p(elf.release(), image.release(), ispc->GetLiveBitmap(), ispc->GetMemMap(),
             delta, true, timings);
  t.NewTiming("Patching files");
  if (!p.PatchElf()) {
    LOG(INFO) << "Failed to patch oat file " << input_oat->GetPath();
    return false;
  }
  if (!p.PatchImage()) {
    LOG(INFO) << "Failed to patch image file " << input_art->GetPath();
    return false;
  }

  t.NewTiming("Writing files");
  if (!p.WriteElf(output_oat)) {
    return false;
  }
  if (!p.WriteImage(output_art)) {
    return false;
  }
  return true;
}

bool PatchOat::WriteElf(File* out) {
  TimingLogger::ScopedTiming t("Writing Elf File", &timings_);
  CHECK(oat_file_.get() != nullptr);
  CHECK(out != nullptr);
  size_t expect = oat_file_->Size();
  if (static_cast<size_t>(out->Write(
            reinterpret_cast<char*>(oat_file_->Begin()), expect, 0)) == expect &&
      out->SetLength(expect) == 0) {
    return true;
  } else {
    LOG(ERROR) << "Writing to oat file " << out->GetPath() << " failed.";
    return false;
  }
}

bool PatchOat::WriteImage(File* out) {
  TimingLogger::ScopedTiming t("Writing image File", &timings_);
  CHECK(image_ != nullptr);
  CHECK(out != nullptr);
  size_t expect = image_->Size();
  if (static_cast<size_t>(out->Write(
            reinterpret_cast<char*>(image_->Begin()), expect, 0)) == expect &&
      out->SetLength(expect) == 0) {
    return true;
  } else {
    LOG(ERROR) << "Writing to image file " << out->GetPath() << " failed.";
    return false;
  }
}

bool PatchOat::PatchImage() {
  ImageHeader* image_header = reinterpret_cast<ImageHeader*>(image_->Begin());
  CHECK_GT(image_->Size(), sizeof(ImageHeader));
  // These are the roots from the original file.
  mirror::Object* img_roots = image_header->GetImageRoots();
  image_header->RelocateImage(delta_);

  VisitObject(img_roots);
  if (!image_header->IsValid()) {
    LOG(ERROR) << "reloaction renders image header invalid";
    return false;
  }

  {
    TimingLogger::ScopedTiming t("Walk Bitmap", &timings_);
    // Walk the bitmap.
    WriterMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
    bitmap_->Walk(PatchOat::BitmapCallback, this);
  }
  return true;
}

bool PatchOat::InHeap(mirror::Object* o) {
  uintptr_t begin = reinterpret_cast<uintptr_t>(heap_->Begin());
  uintptr_t end = reinterpret_cast<uintptr_t>(heap_->End());
  uintptr_t obj = reinterpret_cast<uintptr_t>(o);
  return begin <= obj && obj < end;
}

void PatchOat::PatchVisitor::operator() (mirror::Object* obj, MemberOffset off,
                                         bool is_static_unused) const {
  mirror::Object* referent = obj->GetFieldObject<mirror::Object, kVerifyNone>(off);
  DCHECK(referent == nullptr || patcher_->InHeap(referent)) << "Referent is not in the heap.";
  mirror::Object* moved_object = patcher_->RelocatedAddressOf(referent);
  copy_->SetFieldObjectWithoutWriteBarrier<false, true, kVerifyNone>(off, moved_object);
}

void PatchOat::PatchVisitor::operator() (mirror::Class* cls, mirror::Reference* ref) const {
  MemberOffset off = mirror::Reference::ReferentOffset();
  mirror::Object* referent = ref->GetReferent();
  DCHECK(referent == nullptr || patcher_->InHeap(referent)) << "Referent is not in the heap.";
  mirror::Object* moved_object = patcher_->RelocatedAddressOf(referent);
  copy_->SetFieldObjectWithoutWriteBarrier<false, true, kVerifyNone>(off, moved_object);
}

mirror::Object* PatchOat::RelocatedCopyOf(mirror::Object* obj) {
  if (obj == nullptr) {
    return nullptr;
  }
  DCHECK_GT(reinterpret_cast<uintptr_t>(obj), reinterpret_cast<uintptr_t>(heap_->Begin()));
  DCHECK_LT(reinterpret_cast<uintptr_t>(obj), reinterpret_cast<uintptr_t>(heap_->End()));
  uintptr_t heap_off =
      reinterpret_cast<uintptr_t>(obj) - reinterpret_cast<uintptr_t>(heap_->Begin());
  DCHECK_LT(heap_off, image_->Size());
  return reinterpret_cast<mirror::Object*>(image_->Begin() + heap_off);
}

mirror::Object* PatchOat::RelocatedAddressOf(mirror::Object* obj) {
  if (obj == nullptr) {
    return nullptr;
  } else {
    return reinterpret_cast<mirror::Object*>(reinterpret_cast<byte*>(obj) + delta_);
  }
}

// Called by BitmapCallback
void PatchOat::VisitObject(mirror::Object* object) {
  mirror::Object* copy = RelocatedCopyOf(object);
  CHECK(copy != nullptr);
  if (kUseBakerOrBrooksReadBarrier) {
    object->AssertReadBarrierPointer();
    if (kUseBrooksReadBarrier) {
      mirror::Object* moved_to = RelocatedAddressOf(object);
      copy->SetReadBarrierPointer(moved_to);
      DCHECK_EQ(copy->GetReadBarrierPointer(), moved_to);
    }
  }
  PatchOat::PatchVisitor visitor(this, copy);
  object->VisitReferences<true, kVerifyNone>(visitor, visitor);
  if (object->IsArtMethod<kVerifyNone>()) {
    FixupMethod(static_cast<mirror::ArtMethod*>(object),
                static_cast<mirror::ArtMethod*>(copy));
  }
}

void PatchOat::FixupMethod(mirror::ArtMethod* object, mirror::ArtMethod* copy) {
  // Just update the entry points if it looks like we should.
  // TODO sanity check all the values.
  uintptr_t portable = reinterpret_cast<uintptr_t>(
      object->GetEntryPointFromPortableCompiledCode<kVerifyNone>());
  if (portable != 0) {
    copy->SetEntryPointFromPortableCompiledCode(reinterpret_cast<void*>(portable + delta_));
  }
  uintptr_t quick= reinterpret_cast<uintptr_t>(
      object->GetEntryPointFromQuickCompiledCode<kVerifyNone>());
  if (quick != 0) {
    copy->SetEntryPointFromQuickCompiledCode(reinterpret_cast<void*>(quick + delta_));
  }
  uintptr_t interpreter = reinterpret_cast<uintptr_t>(
      object->GetEntryPointFromInterpreter<kVerifyNone>());
  if (interpreter != 0) {
    copy->SetEntryPointFromInterpreter(
        reinterpret_cast<mirror::EntryPointFromInterpreter*>(interpreter + delta_));
  }

  uintptr_t native_method = reinterpret_cast<uintptr_t>(object->GetNativeMethod());
  if (native_method != 0) {
    copy->SetNativeMethod(reinterpret_cast<void*>(native_method + delta_));
  }

  uintptr_t native_gc_map = reinterpret_cast<uintptr_t>(object->GetNativeGcMap());
  if (native_gc_map != 0) {
    copy->SetNativeGcMap(reinterpret_cast<uint8_t*>(native_gc_map + delta_));
  }
}

bool PatchOat::Patch(File* input_oat, off_t delta, File* output_oat, TimingLogger& timings) {
  CHECK(input_oat != nullptr);
  CHECK(output_oat != nullptr);
  CHECK_GE(input_oat->Fd(), 0);
  CHECK_GE(output_oat->Fd(), 0);
  TimingLogger::ScopedTiming t("Setup Oat File Patching", &timings);

  std::string err;
  std::unique_ptr<ElfFile> elf(ElfFile::Open(const_cast<File*>(input_oat),
                                             PROT_READ | PROT_WRITE, MAP_PRIVATE, &err));
  if (elf.get() == nullptr) {
    LOG(ERROR) << "unable to open oat file " << input_oat->GetPath() << " : " << err;
    return false;
  }

  PatchOat p(elf.release(), delta, true, timings);
  t.NewTiming("Patch Oat file");
  if (!p.PatchElf()) {
    return false;
  }

  t.NewTiming("Writing oat file");
  if (!p.WriteElf(output_oat)) {
    return false;
  }
  return true;
}

bool PatchOat::CheckOatFile() {
  Elf32_Shdr* patches_sec = oat_file_->FindSectionByName(".oat_patches");
  if (patches_sec == nullptr) {
    return false;
  }
  if (patches_sec->sh_type != SHT_OAT_PATCH) {
    return false;
  }
  uintptr_t* patches = reinterpret_cast<uintptr_t*>(oat_file_->Begin() + patches_sec->sh_offset);
  uintptr_t* patches_end = patches + (patches_sec->sh_size/sizeof(uintptr_t));
  Elf32_Shdr* oat_data_sec = oat_file_->FindSectionByName(".rodata");
  Elf32_Shdr* oat_text_sec = oat_file_->FindSectionByName(".text");
  if (oat_data_sec == nullptr) {
    return false;
  }
  if (oat_text_sec == nullptr) {
    return false;
  }
  if (oat_text_sec->sh_offset <= oat_data_sec->sh_offset) {
    return false;
  }

  for (; patches < patches_end; patches++) {
    if (oat_text_sec->sh_size <= *patches) {
      return false;
    }
  }

  return true;
}

static bool IsDynamicPtr(Elf32_Word tag) {
  switch (tag) {
    case DT_HASH:
    case DT_STRTAB:
    case DT_SYMTAB:
    case DT_RELA:
    case DT_INIT:
    case DT_FINI:
    case DT_REL:
    case DT_DEBUG:
    case DT_JMPREL:
      return true;
    default:
      return false;
  }
}

bool PatchOat::PatchElf() {
  TimingLogger::ScopedTiming t("Fixup Elf Headers", &timings_);
  // Fixup Phdr's
  for (unsigned int i = 0; i < oat_file_->GetProgramHeaderNum(); i++) {
    Elf32_Phdr& hdr = oat_file_->GetProgramHeader(i);
    if (hdr.p_vaddr != 0) {
      hdr.p_vaddr += delta_;
    }
    if (hdr.p_paddr != 0) {
      hdr.p_paddr += delta_;
    }
  }
  // Fixup Shdr's
  for (unsigned int i = 0; i < oat_file_->GetSectionHeaderNum(); i++) {
    Elf32_Shdr& hdr = oat_file_->GetSectionHeader(i);
    if (hdr.sh_addr != 0) {
      hdr.sh_addr += delta_;
    }
  }

  // Fixup Dynamics.
  for (Elf32_Word i = 0; i < oat_file_->GetDynamicNum(); i++) {
    Elf32_Dyn& dyn = oat_file_->GetDynamic(i);
    if (IsDynamicPtr(dyn.d_tag)) {
      dyn.d_un.d_ptr += delta_;
    }
  }

  t.NewTiming("Fixup Elf Symbols");
  // Fixup dynsym
  Elf32_Shdr* dynsym_sec = oat_file_->FindSectionByName(".dynsym");
  CHECK(dynsym_sec != nullptr);
  if (!PatchSymbols(dynsym_sec)) {
    return false;
  }

  // Fixup symtab
  Elf32_Shdr* symtab_sec = oat_file_->FindSectionByName(".symtab");
  if (symtab_sec != nullptr) {
    if (!PatchSymbols(symtab_sec)) {
      return false;
    }
  }

  t.NewTiming("Fixup Elf Text Section");
  // Fixup text
  if (!PatchTextSection()) {
    return false;
  }

  return true;
}

static uint8_t GetStType(uint8_t st_info) {
  return st_info & 0xf;
}

bool PatchOat::PatchSymbols(Elf32_Shdr* section) {
  Elf32_Sym* syms = reinterpret_cast<Elf32_Sym*>(oat_file_->Begin() + section->sh_offset);
  const Elf32_Sym* last_sym =
      reinterpret_cast<Elf32_Sym*>(oat_file_->Begin() + section->sh_offset + section->sh_size);
  CHECK_EQ(section->sh_size % sizeof(Elf32_Sym), 0u) << "Symtab section not multiple of symbol size";
  for (; syms < last_sym; syms++) {
    uint8_t sttype = GetStType(syms->st_info);
    Elf32_Word shndx = syms->st_shndx;
    if (shndx != SHN_ABS && shndx != SHN_COMMON && shndx != SHN_UNDEF &&
        (sttype == STT_FUNC || sttype == STT_OBJECT)) {
      CHECK_NE(syms->st_value, 0u);
      syms->st_value += delta_;
    }
  }
  return true;
}

bool PatchOat::PatchTextSection() {
  Elf32_Shdr* patches_sec = oat_file_->FindSectionByName(".oat_patches");
  if (patches_sec == nullptr) {
    LOG(INFO) << ".oat_patches section not found. Aborting patch";
    return false;
  }
  if (check_ && !CheckOatFile()) {
    return false;
  }
  CHECK_EQ(patches_sec->sh_type, SHT_OAT_PATCH) << "Unexpected type of .oat_patches";
  uintptr_t* patches = reinterpret_cast<uintptr_t*>(oat_file_->Begin() + patches_sec->sh_offset);
  uintptr_t* patches_end = patches + (patches_sec->sh_size/sizeof(uintptr_t));
  Elf32_Shdr* oat_text_sec = oat_file_->FindSectionByName(".text");
  CHECK(oat_text_sec != nullptr);
  byte* to_patch = oat_file_->Begin() + oat_text_sec->sh_offset;
  uintptr_t to_patch_end = reinterpret_cast<uintptr_t>(to_patch) + oat_text_sec->sh_size;

  for (; patches < patches_end; patches++) {
    CHECK_LT(*patches, oat_text_sec->sh_size) << "Bad Patch";
    uint32_t* patch_loc = reinterpret_cast<uint32_t*>(to_patch + *patches);
    CHECK_LT(reinterpret_cast<uintptr_t>(patch_loc), to_patch_end);
    *patch_loc += delta_;
  }

  return true;
}

static int orig_argc;
static char** orig_argv;

static std::string CommandLine() {
  std::vector<std::string> command;
  for (int i = 0; i < orig_argc; ++i) {
    command.push_back(orig_argv[i]);
  }
  return Join(command, ' ');
}

static void UsageErrorV(const char* fmt, va_list ap) {
  std::string error;
  StringAppendV(&error, fmt, ap);
  LOG(ERROR) << error;
}

static void UsageError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);
}

static void Usage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);

  UsageError("Command: %s", CommandLine().c_str());
  UsageError("Usage: patchoat [options]...");
  UsageError("");
  UsageError("  --input-oat-file=<file.oat>: Specifies the filename of the oat file to be patched.");
  UsageError("");
  UsageError("  --input-oat-fd=<file-descriptor>: Specifies the file-descriptor of the oat file");
  UsageError("      to be patched.");
  UsageError("");
  UsageError("  --input-art-location=<file.art>: Specifies the location of the art file to be");
  UsageError("      patched. Note this is the location in the same way that oatdump means");
  UsageError("      location so it is the path without the isa directory.");
  UsageError("");
  UsageError("  --output-oat-file=<file.oat>: Specifies the file to write the patched oat file to.");
  UsageError("");
  UsageError("  --output-oat-fd=<file-descriptor>: Specifies the file-descriptor to write the");
  UsageError("      the patched oat file to.");
  UsageError("");
  UsageError("  --output-art-file=<file.art>: Specifies the file to write the patched art file to.");
  UsageError("");
  UsageError("  --output-art-fd=<file-descriptor>: Specifies the file-descriptor to write the");
  UsageError("      the patched art file to.");
  UsageError("");
  UsageError("  --orig-base-offset=<original-base-offset>: Specify the base offset the input file");
  UsageError("      was compiled with. This is needed if one is specifying a --base-offset");
  UsageError("");
  UsageError("  --base-offset=<new-base-offset>: Specify the base offset we will repatch the");
  UsageError("      given files to use. This requires that --orig-base-offset is also given.");
  UsageError("");
  UsageError("  --base-offset-delta=<delta>: Specify the amount to change the old base-offset by.");
  UsageError("      This value may be negative.");
  UsageError("");
  UsageError("  --patched-art-file=<file.art>: Use the same patch delta as was used to patch");
  UsageError("      the given image file.");
  UsageError("");
  UsageError("  --dump-timings: dump out patch timing information");
  UsageError("");
  UsageError("  --no-dump-timings: do not dump out patch timing information");
  UsageError("");

  exit(EXIT_FAILURE);
}

static bool ReadBaseDelta(const char* name, off_t* delta, std::string& err) {
  CHECK(name != nullptr);
  CHECK(delta != nullptr);
  std::unique_ptr<File> file;
  if (OS::FileExists(name)) {
    file.reset(OS::OpenFileForReading(name));
    if (file.get() == nullptr) {
      err = "Failed to open file %s for reading";
      return false;
    }
  } else {
    err = "File %s does not exist";
    return false;
  }
  CHECK(file.get() != nullptr);
  ImageHeader hdr;
  if (sizeof(hdr) != file->Read(reinterpret_cast<char*>(&hdr), sizeof(hdr), 0)) {
    err = "Failed to read file %s";
    return false;
  }
  if (!hdr.IsValid()) {
    err = "%s does not contain a valid image header.";
    return false;
  }
  *delta = hdr.GetPatchDelta();
  return true;
}

static File* CreateOrOpen(const char* name, bool* created) {
  if (OS::FileExists(name)) {
    *created = false;
    return OS::OpenFileReadWrite(name);
  } else {
    *created = true;
    return OS::CreateEmptyFile(name);
  }
}

int patchoat(int argc, char **argv) {
  InitLogging(argv);
  const bool debug = true;
  orig_argc = argc;
  orig_argv = argv;
  TimingLogger timings("patcher", false, false);

  InitLogging(argv);

  // Skip over the command name.
  argv++;
  argc--;

  if (argc == 0) {
    Usage("No arguments specified");
  }

  timings.StartTiming("Patchoat");

  // cmd line args
  std::string input_oat_filename;
  int input_oat_fd = -1;
  std::string input_art_location;
  std::string output_oat_filename;
  int output_oat_fd = -1;
  std::string output_art_filename;
  int output_art_fd = -1;
  uintptr_t base_offset = 0;
  bool base_offset_set = false;
  uintptr_t orig_base_offset = 0;
  bool orig_base_offset_set = false;
  off_t base_delta = 0;
  bool base_delta_set = false;
  std::string patched_art_filename;
  bool dump_timings = kIsDebugBuild;

  for (int i = 0; i < argc; i++) {
    const StringPiece option(argv[i]);
    const bool log_options = false;
    if (log_options) {
      LOG(INFO) << "patchoat: option[" << i << "]=" << argv[i];
    }
    if (option.starts_with("--input-oat-file=")) {
      input_oat_filename = option.substr(strlen("--input-oat-file=")).data();
    } else if (option.starts_with("--input-oat-fd=")) {
      const char* oat_fd_str = option.substr(strlen("--input-oat-fd=")).data();
      if (!ParseInt(oat_fd_str, &input_oat_fd)) {
        Usage("Failed to parse --input-oat-fd argument '%s' as an integer", oat_fd_str);
      }
      if (input_oat_fd < 0) {
        Usage("--input-oat-fd pass a negative value %d", input_oat_fd);
      }
    } else if (option.starts_with("--input-art-location=")) {
      input_art_location = option.substr(strlen("--input-art-location=")).data();
    } else if (option.starts_with("--output-oat-file=")) {
      output_oat_filename = option.substr(strlen("--output-oat-file=")).data();
    } else if (option.starts_with("--output-oat-fd=")) {
      const char* oat_fd_str = option.substr(strlen("--output-oat-fd=")).data();
      if (!ParseInt(oat_fd_str, &output_oat_fd)) {
        Usage("Failed to parse --output-oat-fd argument '%s' as an integer", oat_fd_str);
      }
      if (output_oat_fd < 0) {
        Usage("--output-oat-fd pass a negative value %d", output_oat_fd);
      }
    } else if (option.starts_with("--output-art-file=")) {
      output_art_filename = option.substr(strlen("--output-art-file=")).data();
    } else if (option.starts_with("--output-art-fd=")) {
      const char* art_fd_str = option.substr(strlen("--output-art-fd=")).data();
      if (!ParseInt(art_fd_str, &output_art_fd)) {
        Usage("Failed to parse --output-art-fd argument '%s' as an integer", art_fd_str);
      }
      if (output_art_fd < 0) {
        Usage("--output-art-fd pass a negative value %d", output_art_fd);
      }
    } else if (option.starts_with("--orig-base-offset=")) {
      const char* orig_base_offset_str = option.substr(strlen("--orig-base-offset=")).data();
      orig_base_offset_set = true;
      if (!ParseUint(orig_base_offset_str, &orig_base_offset)) {
        Usage("Failed to parse --orig-base-offset argument '%s' as an uintptr_t",
              orig_base_offset_str);
      }
    } else if (option.starts_with("--base-offset=")) {
      const char* base_offset_str = option.substr(strlen("--base-offset=")).data();
      base_offset_set = true;
      if (!ParseUint(base_offset_str, &base_offset)) {
        Usage("Failed to parse --base-offset argument '%s' as an uintptr_t", base_offset_str);
      }
    } else if (option.starts_with("--base-offset-delta=")) {
      const char* base_delta_str = option.substr(strlen("--base-offset-delta=")).data();
      base_delta_set = true;
      if (!ParseInt(base_delta_str, &base_delta)) {
        Usage("Failed to parse --base-offset-delta argument '%s' as an off_t", base_delta_str);
      }
    } else if (option.starts_with("--patched-art-file=")) {
      patched_art_filename = option.substr(strlen("--patched-art-file=")).data();
    } else if (option == "--dump-timings") {
      dump_timings = true;
    } else if (option == "--no-dump-timings") {
      dump_timings = false;
    } else {
      Usage("Unknown argument %s", option.data());
    }
  }

  {
    uint32_t cnt = 0;
    cnt += (base_delta_set) ? 1 : 0;
    cnt += (base_offset_set && orig_base_offset_set) ? 1 : 0;
    cnt += (!patched_art_filename.empty()) ? 1 : 0;
    if (cnt > 1) {
      Usage("Only one of --base-offset/--orig-base-offset and --base-offset-delta may be used.");
    } else if (cnt == 0) {
      Usage("Must specify --base-offset-delta, --base-offset and --orig-base-offset, or --patched-art-file");
    }
  }

  if (input_oat_filename.empty() && input_oat_fd == -1) {
    Usage("Input oat file must be supplied by either --input-oat-file or --input-oat-fd");
  }

  if (output_oat_filename.empty() && output_oat_fd == -1) {
    Usage("Input oat file must be supplied by either --output-oat-file or --output-oat-fd");
  }

  if (input_art_location.empty() != (output_art_filename.empty() && output_art_fd == -1)) {
    Usage("Either both input and output art must be supplied or niether must be.");
  }

  if (!base_delta_set) {
    if (orig_base_offset_set && base_offset_set) {
      base_delta_set = true;
      base_delta = base_offset - orig_base_offset;
    } else if (!patched_art_filename.empty()) {
      base_delta_set = true;
      std::string err;
      if (!ReadBaseDelta(patched_art_filename.c_str(), &base_delta, err)) {
        Usage(err.c_str(), patched_art_filename.c_str());
      }
    } else {
      if (base_offset_set) {
        Usage("Unable to determine original base offset.");
      } else {
        Usage("Must suppy a desired new offset or delta.");
      }
    }
  }

  if (!IsAligned<kPageSize>(base_delta)) {
    Usage("Base offset/delta must be alligned to a pagesize (0x%08x) boundary.", kPageSize);
  }

  // Do we need to cleanup output files if we fail?
  bool new_art_out = false;
  bool new_oat_out = false;

  std::unique_ptr<File> input_oat;
  std::unique_ptr<File> output_oat;
  std::unique_ptr<File> output_art;

  bool has_art = false;
  if ((!output_art_filename.empty() || output_art_fd != -1) && !input_art_location.empty()) {
    has_art = true;
    CHECK(!input_art_location.empty());

    if (output_art_fd != -1) {
      output_art.reset(new File(output_art_fd, output_art_filename));
    } else {
      CHECK(!output_art_filename.empty());
      output_art.reset(CreateOrOpen(output_art_filename.c_str(), &new_art_out));
    }
  } else {
    CHECK(!(!output_art_filename.empty() || output_art_fd != -1 || !input_art_location.empty()));
    has_art = false;
  }

  if (input_oat_fd != -1) {
    input_oat.reset(new File(input_oat_fd, input_oat_filename));
  } else {
    CHECK(!input_oat_filename.empty());
    input_oat.reset(OS::OpenFileForReading(input_oat_filename.c_str()));
  }

  if (output_oat_fd != -1) {
    output_oat.reset(new File(output_oat_fd, output_oat_filename));
  } else {
    CHECK(!output_oat_filename.empty());
    output_oat.reset(CreateOrOpen(output_oat_filename.c_str(), &new_oat_out));
  }

  auto cleanup = [&output_art_filename, &output_oat_filename,
                  &new_oat_out, &new_art_out, &timings, &dump_timings](bool success) {
    timings.EndTiming();
    if (!success) {
      if (new_oat_out) {
        CHECK(!output_oat_filename.empty());
        unlink(output_oat_filename.c_str());
      }
      if (new_art_out) {
        CHECK(!output_art_filename.empty());
        unlink(output_art_filename.c_str());
      }
    }
    if (dump_timings) {
      LOG(INFO) << Dumpable<TimingLogger>(timings);
    }
  };

  if (input_oat.get() == nullptr) {
    cleanup(false);
    Usage("Input files must exist.");
  }

  if (debug) {
    LOG(INFO) << "moving offset by " << base_delta << " (0x" << std::hex << base_delta << ") bytes";
  }

  bool ret;
  if (has_art) {
    TimingLogger::ScopedTiming pt("patch image and oat", &timings);
    ret = PatchOat::Patch(input_oat.get(), input_art_location, base_delta,
                          output_oat.get(), output_art.get(), timings);
  } else {
    TimingLogger::ScopedTiming pt("patch oat", &timings);
    ret = PatchOat::Patch(input_oat.get(), base_delta, output_oat.get(), timings);
  }
  cleanup(ret);
  return (ret) ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace art

int main(int argc, char **argv) {
  return art::patchoat(argc, argv);
}

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

#include <stddef.h>
#include "translator.h"
#include "dex_instruction.h"
#include "dex_instruction-inl.h"
#include "dex_instruction_list.h"
#include "xlator_relocs.h"
#include <unistd.h>
#include <sys/mman.h>
#include "thread.h"
#include "thread-inl.h"
#include "base/hex_dump.h"

#ifndef NDEBUG
#include "../disassembler/disassembler_arm.h"
#endif

#define DEBUG_LOGS 0

namespace art {
namespace arm {

// Set this to true to get a disassembly listing of the translated chunks
// in the log file.
bool kDisassembleChunk = false;

extern "C" {
#define XLATOR_LABELS(opcode, cname, p, f, r, i, a, v) \
extern uint32_t art_xlate_code_##cname[];\
extern uint32_t art_xlate_reloc_##cname[];
DEX_INSTRUCTION_LIST(XLATOR_LABELS)

// Labels for entry point.
extern uint32_t art_xlate_code_entry[];
extern uint32_t art_xlate_code_entry_end[];
extern uint32_t* art_xlator_helpers[];

// _BACK variants for branches.
extern uint32_t art_xlate_code_GOTO_BACK[];
extern uint32_t art_xlate_code_GOTO_16_BACK[];
extern uint32_t art_xlate_code_GOTO_32_BACK[];

extern uint32_t art_xlate_reloc_GOTO_BACK[];
extern uint32_t art_xlate_reloc_GOTO_16_BACK[];
extern uint32_t art_xlate_reloc_GOTO_32_BACK[];

extern uint32_t art_xlate_code_IF_EQ_BACK[];
extern uint32_t art_xlate_code_IF_NE_BACK[];
extern uint32_t art_xlate_code_IF_LT_BACK[];
extern uint32_t art_xlate_code_IF_GT_BACK[];
extern uint32_t art_xlate_code_IF_LE_BACK[];
extern uint32_t art_xlate_code_IF_GE_BACK[];

extern uint32_t art_xlate_reloc_IF_EQ_BACK[];
extern uint32_t art_xlate_reloc_IF_NE_BACK[];
extern uint32_t art_xlate_reloc_IF_LT_BACK[];
extern uint32_t art_xlate_reloc_IF_GT_BACK[];
extern uint32_t art_xlate_reloc_IF_LE_BACK[];
extern uint32_t art_xlate_reloc_IF_GE_BACK[];

extern uint32_t art_xlate_code_IF_EQZ_BACK[];
extern uint32_t art_xlate_code_IF_NEZ_BACK[];
extern uint32_t art_xlate_code_IF_LTZ_BACK[];
extern uint32_t art_xlate_code_IF_GTZ_BACK[];
extern uint32_t art_xlate_code_IF_LEZ_BACK[];
extern uint32_t art_xlate_code_IF_GEZ_BACK[];

extern uint32_t art_xlate_reloc_IF_EQZ_BACK[];
extern uint32_t art_xlate_reloc_IF_NEZ_BACK[];
extern uint32_t art_xlate_reloc_IF_LTZ_BACK[];
extern uint32_t art_xlate_reloc_IF_GTZ_BACK[];
extern uint32_t art_xlate_reloc_IF_LEZ_BACK[];
extern uint32_t art_xlate_reloc_IF_GEZ_BACK[];

// SLOW variants for DIV and REM.
extern uint32_t art_xlate_code_DIV_INT_SLOW[];
extern uint32_t art_xlate_code_DIV_INT_SLOW_2ADDR[];
extern uint32_t art_xlate_code_REM_INT_SLOW[];
extern uint32_t art_xlate_code_REM_INT_SLOW_2ADDR[];

extern uint32_t art_xlate_reloc_DIV_INT_SLOW[];
extern uint32_t art_xlate_reloc_DIV_INT_SLOW_2ADDR[];
extern uint32_t art_xlate_reloc_REM_INT_SLOW[];
extern uint32_t art_xlate_reloc_REM_INT_SLOW_2ADDR[];

// Negative constants.
extern uint32_t art_xlate_code_CONST_4_NEGATIVE[];
extern uint32_t art_xlate_code_CONST_16_NEGATIVE[];
extern uint32_t art_xlate_reloc_CONST_4_NEGATIVE[];
extern uint32_t art_xlate_reloc_CONST_16_NEGATIVE[];

// Zero constants.
extern uint32_t art_xlate_code_CONST_4_ZERO[];
extern uint32_t art_xlate_code_CONST_16_ZERO[];
extern uint32_t art_xlate_reloc_CONST_4_ZERO[];
extern uint32_t art_xlate_reloc_CONST_16_ZERO[];

// Negative variants for literal instructions.
extern uint32_t art_xlate_code_ADD_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_code_RSUB_INT_NEGATIVE[];
extern uint32_t art_xlate_code_MUL_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_code_DIV_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_code_REM_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_code_DIV_INT_LIT16_SLOW_NEGATIVE[];
extern uint32_t art_xlate_code_REM_INT_LIT16_SLOW_NEGATIVE[];
extern uint32_t art_xlate_code_AND_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_code_OR_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_code_XOR_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_code_SHL_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_code_SHR_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_code_USHR_INT_LIT16_NEGATIVE[];

extern uint32_t art_xlate_code_ADD_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_code_RSUB_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_code_MUL_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_code_DIV_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_code_REM_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_code_DIV_INT_LIT8_SLOW_NEGATIVE[];
extern uint32_t art_xlate_code_REM_INT_LIT8_SLOW_NEGATIVE[];
extern uint32_t art_xlate_code_AND_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_code_OR_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_code_XOR_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_code_SHL_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_code_SHR_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_code_USHR_INT_LIT8_NEGATIVE[];

extern uint32_t art_xlate_reloc_ADD_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_reloc_RSUB_INT_NEGATIVE[];
extern uint32_t art_xlate_reloc_MUL_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_reloc_DIV_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_reloc_REM_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_reloc_DIV_INT_LIT16_SLOW_NEGATIVE[];
extern uint32_t art_xlate_reloc_REM_INT_LIT16_SLOW_NEGATIVE[];
extern uint32_t art_xlate_reloc_AND_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_reloc_OR_INT_LIT16_NEGATIVE[];
extern uint32_t art_xlate_reloc_XOR_INT_LIT16_NEGATIVE[];

extern uint32_t art_xlate_reloc_ADD_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_reloc_RSUB_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_reloc_MUL_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_reloc_DIV_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_reloc_REM_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_reloc_DIV_INT_LIT8_SLOW_NEGATIVE[];
extern uint32_t art_xlate_reloc_REM_INT_LIT8_SLOW_NEGATIVE[];
extern uint32_t art_xlate_reloc_AND_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_reloc_OR_INT_LIT8_NEGATIVE[];
extern uint32_t art_xlate_reloc_XOR_INT_LIT8_NEGATIVE[];
}

#define XLATOR_TABLE(opcode, cname, p, f, r, i, a, v) art_xlate_code_##cname,
static uint32_t* xlator_table[] = {
  DEX_INSTRUCTION_LIST(XLATOR_TABLE)
};


#define XLATOR_RELOC_TABLE(opcode, cname, p, f, r, i, a, v) art_xlate_reloc_##cname,
static uint32_t* xlator_reloc_table[256] = {
  DEX_INSTRUCTION_LIST(XLATOR_RELOC_TABLE)
};


#define XLATOR_FORMAT_TABLE(opcode, cname, p, f, r, i, a, v) #f,
const char* xlator_format_table[] = {
  DEX_INSTRUCTION_LIST(XLATOR_FORMAT_TABLE)
};


TranslatedMethod::~TranslatedMethod() {
  // Unlink the method from the Translator cache.
  if (prev_ == nullptr) {
    // First in list?
    translator_->cache_head_ = next_;
  } else {
    prev_->next_ = next_;
  }

  if (next_ == nullptr) {
    // Last in list?
    translator_->cache_tail_= prev_;
  } else {
    next_->prev_ = prev_;
  }
  if (dexpc_map_size_ > 32) {
    delete[] dexpc_map_;
  }
  delete[] ppc_map_;
}

// Translate a DEX pc value into the address of a program instruction pointer.
// The dexpc_map is an array of words, each bit of which means the following:
// 1: the program pc increments
// 0: the program pc remains the same (within the same instruction).
// Therefore to calculate the offset into the program (and thus the derive the address
// of the program instruction) we count the number of 1 bits in the array up to the
// final partial word.  GCC has an intrinsic for population count so this should be
// very fast.

// If the number of dex instructions is less than 33 then we use the small_map_ inside
// the TranslatedMethod directly.  This saves unnecessary allocations for a lot of methods.
uint32_t** TranslateDexPC(TranslatedMethod* meth, uint32_t dexpc) {
  if (dexpc >= meth->dexpc_map_size_) {
    LOG(ERROR) << "dex pc is too large: max: " << meth->dexpc_map_size_;
    return nullptr;
  }
  int popcount = 0;
  int bitoffset = dexpc & 0x1f;     // Number of bits in final word.
  int mask = bitoffset == 31 ? 0xffffffff : (1 << (bitoffset + 1)) - 1;
  if (meth->dexpc_map_size_ <= 32) {
    popcount += __builtin_popcount(meth->small_map_ & mask);
  } else {
    int wordoffset = dexpc >> 5;      // Number of whole words.
    for (int i = 0; i < wordoffset; ++i) {
      popcount += __builtin_popcount(meth->dexpc_map_[i]);
    }
    popcount += __builtin_popcount(meth->dexpc_map_[wordoffset] & mask);
  }
#if DEBUG_LOGS
  LOG(INFO) << "translating dexpc " << dexpc << " into ppc " << meth->program_[popcount];
#endif
  return &meth->program_[popcount];
}

// Given the processor PC, translate it to a dex pc using the
// information in the TranslatedMethod.
//
// In the TranslatedMethod there is an array of addresses (the program_).  This is
// a sorted list of the addresses of the start of each translated dex instruction.  The
// processor PC value will be inside one of these regions.
uint32_t LookupDexPC(TranslatedMethod* meth, uint32_t pc) {
#if DEBUG_LOGS
  LOG(INFO) << "Looking up dex pc from pc " << std::hex << pc;
#endif
  int lo = 0;
  int end = meth->index_size_ - 1;
  int hi = end;
  uint32_t* index = reinterpret_cast<uint32_t*>(meth->program_);
  uint32_t endindex = reinterpret_cast<uint32_t>(meth->end_program_);
  uint32_t pe;
  while (lo <= hi) {
    int mid = (hi + lo)/2;
    int next = mid + 1;
    if (next >= end) {
      pe = endindex;
    } else {
      pe = index[next];
    }
#if DEBUG_LOGS
    LOG(INFO) << "looking at " << std::hex << index[mid] << "..." << pe;
#endif
    if (pc > index[mid] && pc < pe) {
      // mid is the index into the program.  Use the ppc_map to translate this
      // into a dex pc.
      uint32_t dexpc = 0;
      for (int i = 0; i < mid; i++) {
        dexpc += meth->ppc_map_[i];
      }
      // In this instruction.
#if DEBUG_LOGS
      LOG(INFO) << "found " << dexpc;
#endif
      return dexpc;
    }
    if (pc < index[mid]) {
      hi = mid - 1;
    } else {
      lo = mid + 1;
    }
  }
#if DEBUG_LOGS
  LOG(INFO) << "not found";
#endif
  return -1;
}


void Translator::CacheMethod(TranslatedMethod* method) {
  MutexLock mu(Thread::Current(), lock_);
  cache_size_ += method->program_size_;

  // Add it to the end of the cache list.
  if (cache_head_ == nullptr) {
    CHECK(cache_tail_ == nullptr);
    cache_head_ = cache_tail_ = method;
  } else {
    CHECK(cache_tail_ != nullptr);
    cache_tail_->next_ = method;
    method->prev_ = cache_tail_;
    cache_tail_ = method;
  }
}

void Translator::ShowCacheSize() {
  LOG(INFO) << "Translator cache size is " << PrettySize(cache_size_);
}

bool Translator::MakeRoomInCache(uint32_t sizeneeded) {
  if (sizeneeded > kCacheSizeInBytes) {
    // Method is too big, no way to translate it.
    // LOG(INFO) << "Method is too big for cache, bailing...";
    // return false;
    LOG(INFO) << "Method is big: " << PrettySize(sizeneeded);
    ShowCacheSize();
  }
  MutexLock mu(Thread::Current(), lock_);
  uint32_t newsize = cache_size_ + sizeneeded;
  if (newsize > kCacheSizeInBytes) {
    // Need to make space in cache.
    // Scan from the cache_tail backwards.
    if (false) {
      TranslatedMethod* meth = cache_tail_;

      // TODO: implement this.  Problems to solve:
      // 1. find the best methods to remove
      // 2. make sure the method is not currently executing in any thread
      while (meth != nullptr) {
        meth = meth->prev_;
      }
    }
  }
  return true;
}

Translator::Translator(ChunkTable& chunk_table, const uint32_t hsize) : chunk_table_(chunk_table),
  end_element_(nullptr), end_pool_(nullptr), lock_("Translator"), cache_size_(0),
  cache_head_(nullptr), cache_tail_(nullptr), helper_size_(hsize) {
}



void Translator::MakeExecutable(void* addr, uint32_t len) {
  const int32_t kPageSize = 4096;
  int8_t* aligned_start = reinterpret_cast<int8_t*>(reinterpret_cast<uint32_t>(addr)
    & ~(kPageSize - 1));

  int8_t* aligned_end = reinterpret_cast<int8_t*>(reinterpret_cast<uint32_t>((
     reinterpret_cast<uint32_t>(addr) + len) +
       (kPageSize - 1)) & ~(kPageSize - 1));

  // LOG(INFO) << "making executable " << addr << " for " << len << " bytes";
  // LOG(INFO) << "mprotecting " << reinterpret_cast<void*>(aligned_start) << " to " << reinterpret_cast<void*>(aligned_end);

  mprotect(aligned_start, aligned_end - aligned_start, PROT_WRITE|PROT_READ|PROT_EXEC);

  cacheflush(reinterpret_cast<int32_t>(addr), reinterpret_cast<int32_t>(addr) +
              len + sizeof(uint32_t), 0);
}

ARMTranslator::ARMTranslator(ChunkTable& chunk_table, const uint32_t hsize)
    :Translator(chunk_table, hsize) {
}


void ARMTranslator::AllocateHelperTrampoline(uint32_t* trampoline) {
  MutexLock mu(Thread::Current(), lock_);
  // LOG(INFO) << "First helper is at " << std::hex << art_xlator_helpers[0];
  int helper_size_in_words = helper_size_ / sizeof(uint32_t);
  // LOG(INFO) << "Allocating helper trampolines at address " << trampoline;
  for (int i = 0; i < helper_size_in_words; i++) {
    // Build an instruction to set the PC to the helper address
    uint32_t instr = 0xe596f000;      // ldr pc,[r6,#0]
    instr |= i * 4;
    trampoline[i] = instr;
  }
  helper_trampolines_.push_back(trampoline);
}

uint32_t ARMTranslator::FindHelperTrampoline(uint32_t pc, uint32_t helper) {
  // LOG(INFO) << "Looking for helper trampoline for pc " << std::hex << pc;
  MutexLock mu(Thread::Current(), lock_);
  uint32_t helperaddr = reinterpret_cast<uint32_t>(art_xlator_helpers[helper]);
  if (!IsInBLRange(pc, helperaddr)) {
    for (size_t i = 0; i < helper_trampolines_.size(); ++i) {
      helperaddr = reinterpret_cast<uint32_t>(&helper_trampolines_[i][helper]);
      // LOG(INFO) << "helper addr " << std::hex << helperaddr;
      if (IsInBLRange(pc, helperaddr)) {
        // LOG(INFO) << "returning trampoline helper " << std::hex << helperaddr;
        return helperaddr;
      }
    }
  } else {
    // LOG(INFO) << "returning main helper " << std::hex << helperaddr;
    return helperaddr;
  }
  // LOG(INFO) << "not found";
  return 0;
}

void Translator::DeleteMethod(TranslatedMethod* method) {
  uint32_t* entrypoint = art_xlate_code_entry;
  int entrypoint_size_in_words = art_xlate_code_entry_end - entrypoint;

  uint32_t startaddr = reinterpret_cast<uint32_t>(method) -
      entrypoint_size_in_words * sizeof(uint32_t) + sizeof(TranslatedMethod);

  // Direct destructor call to the TranslatedMethod.   This will free up any allocated
  // data within it and remove it from the list.
  method->~TranslatedMethod();

  // We're done with the method now.  Remove it.
  delete[] reinterpret_cast<uint32_t*>(startaddr);
}

void Translator::Clear() {
  while (cache_head_ != nullptr) {
    TranslatedMethod* method = cache_head_;
    DeleteMethod(method);
  }
  cache_size_ = 0;
}

// Translate a DEX instruction sequence into something that can be called natively.  The
// result is the address of an executable sequence of code laid out as follows:
//
//          +-----------------+
//          |  entrypoint     |   <- hardcoded assembly language for setup
//          |                 |
//          +-----------------+
//          |  helpers        |   <- address of array of helper routines
//          +-----------------+
//          | TranslatedMethod|   <- data structure
//          +-----------------+
//          | program         |
//          | index           |   <- array of addresses into program for each dex instruction
//          |                 |
//          +-----------------+
//          |                 |
//          |                 |   <- translated dex instructions
//          |      program    |
//          |                 |
//          +-----------------+
//          |    helper       |
//          |    trampoline   |
//          | (if necessary)  |
//          +-----------------+

// 'entrypoint' is a piece of code whose source is art_xlate_code_entry (in xlate.S).  This
// makes the stack frame and sets up the registers.  It then invokes the first 'instruction'
// of the translated DEX program.
//
// 'TranslatedMethod' is an instance of a 'TranslatedMethod' struct (defined in translate.h).
// This contains a pointer to the program and a 'dex PC map' used to translate a DEX pc value
// into a 'program pc' value for exception handling
//
// 'program index' is a sequence of addresses (word sized).  At these addresses is located the
// executable translation of a specific DEX instruction.

// The helper trampolines are computed branches to helper functions.  They are located
// at the end of the program so that they are within the range of a call instruction
// in the method.  They may be absent if there is a trampoline that is already in range.

// TODO: need make this more generic.  In particular, the use of uint32_t* for the code
// pointers makes is not usable for non-word-aligned processors (e.g. Intel).

mirror::EntryPointFromInterpreter* Translator::Translate(mirror::ArtMethod* const method,
                                                            const uint16_t *code,
                                                            const uint16_t *endcode) {
#if DEBUG_LOGS
  LOG(INFO) << "Translator starting to translate: " << code << "..." << endcode;
#endif

  int ppc = 0;    // Program pseudo PC (offset into program in words).
  int dexpc = 0;  // Program counter into dex code (offset into method code).

  // Get the entry point code
  uint32_t* entrypoint = art_xlate_code_entry;
  int entrypoint_size_in_words = art_xlate_code_entry_end - entrypoint;

  // For translation of branches we need to know the program PC (ppc) value for
  // each dex pc (dexpc) value.
  int offset_map_size = endcode - code;
  uint32_t *offset_map = new uint32_t[offset_map_size];

  // Make a pass through the dex instructions, counting the quantity of them and
  // building up the offset map and ppc map.
  const uint16_t *pc = code;
  int num_instructions = 0;
  while (pc < endcode) {
    uint16_t inst = *pc;
    uint8_t opcode = inst & 0xff;
    if (IsData(inst)) {
      int size = 0;
      // Switch or filled array data.
      switch (inst) {
        case Instruction::kPackedSwitchSignature:
          // Packed switch.
          size = pc[1] * 2 + 4;
          break;
        case Instruction::kSparseSwitchSignature:
          // Sparse switch.
          size = pc[1] * 4 + 2;
          break;
        case Instruction::kArrayDataSignature:
          // Filled array data.
          size = (pc[1] * pc[2] + 1) / 2 + 4;
          break;
      }
      pc += size;
      dexpc += size;
      // No instruction counted for this.
    } else {
      int size = InstructionSize(opcode);
      for (int i = 0; i < size; ++i) {
        offset_map[dexpc] = num_instructions;
        ++dexpc;
      }
      pc += size;
      ++num_instructions;
    }
  }



  // Now that we know the number of instructions, work out how big the
  // program will be.  For each instruction, look up the code and reloc
  // from the tables.  The difference between them is the number of
  // words in the instruction. We use this to build up an index
  // of offsets to the start of the instructions.

  // Build the ppc map.  This allows for the translation of a processor pc into a dex pc.
  // Each entry in the program index strides one or more dex pc values.  This map
  // is an array of bytes, with each entry being the number of dex instructions straddled
  // by the translated instruction.

  uint32_t* index = new uint32_t[num_instructions];
  uint8_t* ppc_map = new uint8_t[num_instructions];

  int indexi = 0;
  int indexoffset = 0;

  pc = code;
  while (pc < endcode) {
    uint16_t rawinst = *pc;
    const Instruction* inst = Instruction::At(pc);
    uint8_t opcode = rawinst & 0xff;
    if (IsData(rawinst)) {
      int size = 0;
      // Switch or filled array data.
      switch (rawinst) {
        case Instruction::kPackedSwitchSignature:
          // Packed switch.
          size = pc[1] * 2 + 4;
          break;
        case Instruction::kSparseSwitchSignature:
          // Sparse switch.
          size = pc[1] * 4 + 2;
          break;
        case Instruction::kArrayDataSignature:
          // Filled array data.
          size = (pc[1] * pc[2] + 1) / 2 + 4;
          break;
      }
      pc += size;
    } else if (IsSpecial(inst)) {
      int size = InstructionSize(opcode);
      int size_in_words = SizeofSpecial(inst);
      index[indexi] = indexoffset;
      ppc_map[indexi] = size;

      ++indexi;
      indexoffset += size_in_words;
      pc += size;
     } else {
      int size = InstructionSize(opcode);
      uint32_t *chunk = xlator_table[opcode];
      uint32_t *relocs = xlator_reloc_table[opcode];
      uint32_t attrs = chunk[-1];         // Attributes precede chunk.
      int size_in_words = 0;
      if ((attrs & kAttrShared) != 0) {
        size_in_words = 1;            // Call instruction (TODO: not right for Intel).
      } else {
        size_in_words = relocs - chunk;
      }
      index[indexi] = indexoffset;
      ppc_map[indexi] = size;

      ++indexi;
      indexoffset += size_in_words;
      pc += size;
    }
  }


  // LOG(INFO) << "offset map: " << HexDump(offset_map, (endcode - code) * sizeof(int), true);
  // LOG(INFO) << "allocating program for " << num_instructions << " instructions";
  // We now know the number of dex instructions so we can allocate the program.
  // The program contains space for the entry point at the beginning and the translated
  // dex instructions at the end.
  int helper_trampoline_size = 0;
  uint32_t* program = nullptr;
  int programsize = 0;      // In Words.
  int program_size_in_bytes;

  // We may need to allocate a helper trampoline at the end of the program.  We only know this
  // once we know the address of the program itself so we might need to try to allocate the program
  // twice.  The first time to get the address and the second time with a trampoline at the end.
  // The second allocation is guaranteed to succeed since the trampoline is very close to the
  // program.
  for (int i = 0; i < 2; i++) {
    programsize = entrypoint_size_in_words + num_instructions +
        indexoffset +  helper_trampoline_size;  // Words.
    program_size_in_bytes = programsize * sizeof(uint32_t);
    // LOG(INFO) << "Program size is " << program_size_in_bytes;

    // Make some room in the cache for this program.  If there is no room this will delete
    // a translated method (if it can).  A return value of false indicates that there is no
    // way to translate this method.
    if (!MakeRoomInCache(program_size_in_bytes)) {
      delete[] index;
      delete[] ppc_map;
      return nullptr;
    }

    // Start address for program (entrypoint).
    program = new uint32_t[programsize];
    if (helper_trampoline_size > 0) {
      // There is a helper trampoline at the end of this program.  Fill it in.
      uint32_t* trampoline = program + programsize - helper_trampoline_size;
      AllocateHelperTrampoline(trampoline);
    }
    // See if we need to allocate a helper trampoline.
    uint32_t helperaddr = FindHelperTrampoline(reinterpret_cast<uint32_t>(program), 0);
    // LOG(INFO) << "helper trampoline is " << std::hex << helperaddr;
    if (helperaddr != 0) {
      break;
    }
    delete[] program;
    program = nullptr;
    helper_trampoline_size = helper_size_ / sizeof(uint32_t);
  }

  if (program == nullptr) {
    // Failed to allocate helper trampoline.
    LOG(INFO) << "Failed to allocate helper trampoline";
    delete[] index;
    delete[] ppc_map;
    return nullptr;
  }

  // Address of program index (immediately after header).
  uint32_t** next_pindex = reinterpret_cast<uint32_t**>(program) + entrypoint_size_in_words;
  uint32_t** pindex = next_pindex;

  // Address of first translated instruction.
  uint32_t* next_pinst = program + entrypoint_size_in_words + num_instructions;

#ifndef NDEBUG
  uint32_t* codestart = next_pinst;
  uint32_t* codeend = codestart + indexoffset;
#endif

  // Copy in the entrypoint at the start of the program.
  memcpy(program, entrypoint, entrypoint_size_in_words * sizeof(uint32_t));

  // The TranslatedMethod is located at the end of the entrypoint.
  TranslatedMethod *txmethod = reinterpret_cast<TranslatedMethod*>(program +
      entrypoint_size_in_words -
      (sizeof(TranslatedMethod) / sizeof(uint32_t)));


  // Write in the address of the art_xlator_helpers array.  This is located immediately
  // before the TranslatedMethod.
  uint32_t** helpers = reinterpret_cast<uint32_t**>(txmethod) - 1;
  *helpers = reinterpret_cast<uint32_t*>(art_xlator_helpers);

  // Now we know the address of the program.  We can convert the index into real
  // addresses and write them to the program.

  for (int i = 0; i < num_instructions; i++) {
    *next_pindex = next_pinst + index[i];
#if DEBUG_LOGS
    LOG(INFO) << "index[" << i << "]: " << *next_pindex;
#endif
    ++next_pindex;
  }

  // We're done with the index.
  delete[] index;

  // Now pass through doing the translations.  Each instruction is copied into the
  // next_pinst address.  Then next_pinst is incremented by the number of words in
  // the instruction.
  pc = code;
  dexpc = 0;

  bool ok = true;

  while (ok && pc < endcode) {
    const Instruction* inst = Instruction::At(pc);
#if DEBUG_LOGS
    LOG(INFO) << "translating instruction " << inst->DumpString(nullptr);
#endif

    uint16_t inst_data = inst->Fetch16(0);
    uint8_t opcode = inst_data & 0xff;

    if (IsData(inst_data)) {
      // Switch or filled array data.
      int size = 0;
      switch (inst_data) {
         case Instruction::kPackedSwitchSignature:
           // Packed switch.
           size = pc[1] * 2 + 4;
           break;
         case Instruction::kSparseSwitchSignature:
           // Sparse switch.
           size = pc[1] * 4 + 2;
           break;
         case Instruction::kArrayDataSignature:
           // Filled array data.
           size = (pc[1] * pc[2] + 1) / 2 + 4;
           break;
       }
      pc += size;   // Skip to end of special data.
    } else if (IsSpecial(inst)) {
#if DEBUG_LOGS
      LOG(INFO) << "translating special instruction";
#endif
      // Opcode that needs special handling.  Also update PC values.
      ok = TranslateSpecial(inst, pc, ppc, dexpc, offset_map, next_pinst, pindex);
    } else {
#if DEBUG_LOGS
      LOG(INFO) << "translating general instruction";
#endif
      // General, relocated case.
      int isize = InstructionSize(opcode);

      uint32_t *chunk = xlator_table[opcode];
      uint32_t *relocs = xlator_reloc_table[opcode];
      uint32_t attrs = chunk[-1];         // Attributes precede chunk.
      if ((attrs & kAttrShared) != 0) {
        ok = AddSharedChunk(inst, pc, chunk, relocs, isize, next_pinst, dexpc);
      } else {
        ok = AddChunk(inst, pc, chunk, relocs, isize, next_pinst, dexpc);
      }

      pc += isize;
      dexpc += isize;
      ppc++;
    }
  }

  if (!ok) {
    // Something went wrong.  Bail and let the portable interpreter handle it.
    delete[] program;
    delete[] offset_map;
    delete[] ppc_map;
    return nullptr;
  }

  // TODO: enable this when it can do something useful.
  // program = PeepHole(program, entrypoint_size_in_words);

  // Fill in the TranslatedMethod object at the end of the entrypoint.
  txmethod->program_ = reinterpret_cast<uint32_t**>(program) + entrypoint_size_in_words;
  txmethod->dexpc_map_size_ = offset_map_size;    // Number of dex instructions.
  txmethod->end_program_ = reinterpret_cast<uint32_t**>(program) + programsize;
  txmethod->index_size_ = num_instructions;
  txmethod->ppc_map_ = ppc_map;
  txmethod->translator_ = this;
  txmethod->next_ = nullptr;
  txmethod->prev_ = nullptr;
  txmethod->program_size_ = program_size_in_bytes;
  txmethod->method_ = method;
  txmethod->entry_count_ = 0;

  // Transform and compress the offset map into a dexpc map.  In this form
  // we use a bit map.  A 1 bit means that the program PC increments by one, a zero
  // bit means it's the same.  We start at the LSB as the first bit.  We can then
  // use a population count operation to calculate the increment from the start of the
  // program for any given DEX pc.

  // Optimization here.  A lot of methods are smaller than 32 instructions so we use the
  // memory inside the TranslatedMethod directly for the dex pc map for those ones.  For
  // larger ones we allocate an array and point to it.
  int dexpcmap_size = (offset_map_size - 1) / 32 + 1;
  uint32_t* dexpcmap = nullptr;
  if (offset_map_size <= 32) {
    dexpcmap = &txmethod->small_map_;
  } else {
    dexpcmap = new uint32_t[dexpcmap_size];
    txmethod->dexpc_map_ = dexpcmap;
  }

  uint32_t lastoffset = 0;
  int dexpcmap_bit = 0;
  uint32_t dexpc_accumulator = 0;
  int dexpcmap_index = 0;
  for (int i = 0; i < offset_map_size; ++i) {
    // Move to next word if we have filled the current one.
    if (dexpcmap_bit > 31) {
        dexpcmap[dexpcmap_index] = dexpc_accumulator;   // Commit accumulator.
        dexpc_accumulator = 0;                          // And reset it.
        ++dexpcmap_index;                               // Use next index.
        dexpcmap_bit = 0;                               // Start again at LSB.
    }

    // Add a 1 bit if the offset is different from last time.
    int bit = offset_map[i] != lastoffset;
    dexpc_accumulator |= (bit << dexpcmap_bit);
    ++dexpcmap_bit;
    lastoffset = offset_map[i];
  }



  // Finally commit the last word if there is one.
  if (dexpcmap_index < dexpcmap_size) {
    dexpcmap[dexpcmap_index] = dexpc_accumulator;
  }
  delete[] offset_map;      // We don't need this now that we have the dexpcmap.

  MakeExecutable(program, program_size_in_bytes);

  // LOG(INFO) << "program:\n" << HexDump(program, num_instructions*4, true, "program ");
#if DEBUG_LOGS
  LOG(INFO) << HexDump(program, entrypoint_size_in_words*sizeof(uint32_t), true, "program ");
  uint32_t** program_table = reinterpret_cast<uint32_t**>(program) + entrypoint_size_in_words;
  for (int i = 0; i < num_instructions; i++) {
    LOG(INFO) << i << ": " << &program_table[i] << ": " << program_table[i];
  }
#endif

  // Everything ok, cache the method.
  CacheMethod(txmethod);

#ifndef NDEBUG
  if (kDisassembleChunk) {
    // Disassemble the output if in debug mode.
    DisassemblerArm dasm;
    LOG(INFO) << "Disassembly of method " << PrettyMethod(method, true);
    uint32_t i = 0;
    uint32_t *inststart = pindex[i];
    pc = code;
    for (uint32_t* p = codestart; p < codeend; p++) {
      if (p == inststart) {
        // This is the start of a DEX instruction
        const Instruction* inst = Instruction::At(pc);
        LOG(INFO) << "DEX Instruction: " << inst->DumpString(nullptr);
        int size = InstructionSize(inst->Opcode());
        pc += size;
        ++i;
        inststart = pindex[i];
      }
      dasm.Dump(LOG(INFO), reinterpret_cast<uint8_t*>(p));
    }
    LOG(INFO) << "End of disassembly";
  }
#endif

  return reinterpret_cast<mirror::EntryPointFromInterpreter*>(program);
}

static uint32_t ApplyRelocModifier(uint32_t value, int mode) {
  switch (mode) {
    case kRelocAll: return value;
    case kRelocLo: return value & 0xffff;
    case kRelocHi: return (value >> 16) & 0xffff;
    case kRelocNegative: return -value;
    case kRelocMult4: return value * 4;
    case kRelocRaw: return value;
    default:
      return value;
  }
}

static uint32_t ApplyModifierHighWord(uint64_t value, int mode) {
  value >>= 32LL;
  switch (mode) {
    case kRelocAll: return value;
    case kRelocLo: return value & 0xffff;
    case kRelocHi: return (value >> 16) & 0xffff;
    case kRelocNegative: return -value;
    case kRelocMult4: return value * 4;
    case kRelocRaw: return value;
    default:
      return value;
  }
}
inline int SignExtend4(int v) {
  return (v << 28) >> 28;
}

inline int SignExtend8(int v) {
  return (v << 24) >> 24;
}

inline int SignExtend16(int v) {
  return (v << 16) >> 16;
}


// Macro to save error prone duplicate typing.
#define CASE_IF_XX(XX) \
    Instruction:: XX:\
    offset = SignExtend16(inst->VRegC_22t());\
    if (offset < 0) {\
      chunk = art_xlate_code_##XX##_BACK;\
      relocs = art_xlate_reloc_##XX##_BACK;\
    } else {\
      chunk = art_xlate_code_##XX;\
      relocs = art_xlate_reloc_##XX;\
    }\
    break

#define CASE_IF_XXZ(XX) \
    Instruction:: XX:\
    offset = SignExtend16(inst->VRegB_21t());\
    if (offset < 0) {\
      chunk = art_xlate_code_##XX##_BACK;\
      relocs = art_xlate_reloc_##XX##_BACK;\
    } else {\
      chunk = art_xlate_code_##XX;\
      relocs = art_xlate_reloc_##XX;\
    }\
    break

static uint32_t* negative_opcodes_code[] = {
    art_xlate_code_ADD_INT_LIT16_NEGATIVE,
    art_xlate_code_RSUB_INT_NEGATIVE,
    art_xlate_code_MUL_INT_LIT16_NEGATIVE,
    art_xlate_code_DIV_INT_LIT16_NEGATIVE,
    art_xlate_code_REM_INT_LIT16_NEGATIVE,
    art_xlate_code_AND_INT_LIT16_NEGATIVE,
    art_xlate_code_OR_INT_LIT16_NEGATIVE,
    art_xlate_code_XOR_INT_LIT16_NEGATIVE,

    art_xlate_code_ADD_INT_LIT8_NEGATIVE,
    art_xlate_code_RSUB_INT_LIT8_NEGATIVE,
    art_xlate_code_MUL_INT_LIT8_NEGATIVE,
    art_xlate_code_DIV_INT_LIT8_NEGATIVE,
    art_xlate_code_REM_INT_LIT8_NEGATIVE,
    art_xlate_code_AND_INT_LIT8_NEGATIVE,
    art_xlate_code_OR_INT_LIT8_NEGATIVE,
    art_xlate_code_XOR_INT_LIT8_NEGATIVE,
    nullptr,      // No negative shifts.
    nullptr,
    nullptr,
};

static uint32_t* negative_opcodes_relocs[] = {
    art_xlate_reloc_ADD_INT_LIT16_NEGATIVE,
    art_xlate_reloc_RSUB_INT_NEGATIVE,
    art_xlate_reloc_MUL_INT_LIT16_NEGATIVE,
    art_xlate_reloc_DIV_INT_LIT16_NEGATIVE,
    art_xlate_reloc_REM_INT_LIT16_NEGATIVE,
    art_xlate_reloc_AND_INT_LIT16_NEGATIVE,
    art_xlate_reloc_OR_INT_LIT16_NEGATIVE,
    art_xlate_reloc_XOR_INT_LIT16_NEGATIVE,

    art_xlate_reloc_ADD_INT_LIT8_NEGATIVE,
    art_xlate_reloc_RSUB_INT_LIT8_NEGATIVE,
    art_xlate_reloc_MUL_INT_LIT8_NEGATIVE,
    art_xlate_reloc_DIV_INT_LIT8_NEGATIVE,
    art_xlate_reloc_REM_INT_LIT8_NEGATIVE,
    art_xlate_reloc_AND_INT_LIT8_NEGATIVE,
    art_xlate_reloc_OR_INT_LIT8_NEGATIVE,
    art_xlate_reloc_XOR_INT_LIT8_NEGATIVE,
    nullptr,      // No negative shifts.
    nullptr,
    nullptr,
};

int ARMTranslator::SizeofSpecial(const Instruction* inst) {
  Instruction::Code opcode = inst->Opcode();
  uint16_t inst_data = inst->Fetch16(0);
  uint32_t* chunk = xlator_table[opcode];
  uint32_t* relocs = xlator_reloc_table[opcode];

  if (inst->IsBranch()) {
    int offset = 0;
    switch (opcode) {
      case Instruction::GOTO:
        offset = SignExtend8(inst->VRegA_10t(inst_data));
        if (offset < 0) {
          chunk = art_xlate_code_GOTO_BACK;
          relocs = art_xlate_reloc_GOTO_BACK;
        } else {
          chunk = art_xlate_code_GOTO;
          relocs = art_xlate_reloc_GOTO;
        }
        break;
      case Instruction::GOTO_16:
        offset = SignExtend16(inst->VRegA_20t());
        if (offset < 0) {
          chunk = art_xlate_code_GOTO_16_BACK;
          relocs = art_xlate_reloc_GOTO_16_BACK;
        } else {
          chunk = art_xlate_code_GOTO_16;
          relocs = art_xlate_reloc_GOTO_16;
        }
        break;
      case Instruction::GOTO_32:
        offset = inst->VRegA_30t();
        if (offset <= 0) {     // NB: this can branch to itself
          chunk = art_xlate_code_GOTO_32_BACK;
          relocs = art_xlate_reloc_GOTO_32_BACK;
        } else {
          chunk = art_xlate_code_GOTO_32;
          relocs = art_xlate_reloc_GOTO_32;
        }
        break;

        // NOLINT(whitespace/braces)
      case CASE_IF_XX(IF_EQ);
      case CASE_IF_XX(IF_NE);
      case CASE_IF_XX(IF_LT);
      case CASE_IF_XX(IF_GT);
      case CASE_IF_XX(IF_LE);
      case CASE_IF_XX(IF_GE);

      case CASE_IF_XXZ(IF_EQZ);
      case CASE_IF_XXZ(IF_NEZ);
      case CASE_IF_XXZ(IF_LTZ);
      case CASE_IF_XXZ(IF_GTZ);
      case CASE_IF_XXZ(IF_LEZ);
      case CASE_IF_XXZ(IF_GEZ);

      default:
        LOG(FATAL) << "Unhandled branch instruction";
    }
  } else if (inst->IsSwitch()) {
  } else if (opcode == Instruction::FILL_ARRAY_DATA) {
  } else if (opcode >= Instruction::ADD_INT_LIT16 && opcode <= Instruction::USHR_INT_LIT8) {
    int val = 0;
    if (opcode >= Instruction::ADD_INT_LIT8) {
      val = SignExtend8(inst->VRegC_22b());
    } else {
      val = SignExtend16(inst->VRegC_22s());
    }
    // TODO: handle the SLOW DIV and REM variants.
    if (val < 0) {
      int index = opcode - Instruction::ADD_INT_LIT16;
      chunk = negative_opcodes_code[index];
      relocs = negative_opcodes_relocs[index];
    }
  } else {
    switch (opcode) {
      case Instruction::DIV_INT:
        if (false) {
          chunk = art_xlate_code_DIV_INT_SLOW;
        }
        break;
      case Instruction::DIV_INT_2ADDR:
        if (false) {
          chunk = art_xlate_code_DIV_INT_SLOW_2ADDR;
        }
        break;
      case Instruction::REM_INT:
        if (false) {
          chunk = art_xlate_code_REM_INT_SLOW;
        }
        break;
      case Instruction::REM_INT_2ADDR:
        if (false) {
          chunk = art_xlate_code_REM_INT_SLOW_2ADDR;
        }
        break;
      case Instruction::CONST_4: {
        int val = SignExtend4(inst->VRegB_11n(inst_data));
        if (val == 0) {
          chunk = art_xlate_code_CONST_4_ZERO;
          relocs = art_xlate_reloc_CONST_4_ZERO;
        } else if (val < 0) {
          chunk = art_xlate_code_CONST_4_NEGATIVE;
          relocs = art_xlate_reloc_CONST_4_NEGATIVE;
        }
        break;
      }
      case Instruction::CONST_16: {
        int val = SignExtend16(inst->VRegB_21s());
        if (val == 0) {
          chunk = art_xlate_code_CONST_16_ZERO;
          relocs = art_xlate_reloc_CONST_16_ZERO;
        } else if (val < 0) {
          chunk = art_xlate_code_CONST_16_NEGATIVE;
          relocs = art_xlate_reloc_CONST_16_NEGATIVE;
       }
       break;
      }
      default:
        LOG(FATAL) << "Unhandled special dex instruction";
    }
  }

  return relocs - chunk;
}


bool ARMTranslator::TranslateSpecial(const Instruction* inst, const uint16_t*& pc, int& ppc,
                                       int& dexpc,
                                       uint32_t* offset_map, uint32_t*& mem, uint32_t** index) {
  Instruction::Code opcode = inst->Opcode();
  uint16_t inst_data = inst->Fetch16(0);
  uint32_t* chunk = nullptr;
  uint32_t* relocs = nullptr;

  if (inst->IsBranch()) {
    int offset = 0;
    switch (opcode) {
      case Instruction::GOTO:
        offset = SignExtend8(inst->VRegA_10t(inst_data));
        if (offset < 0) {
          chunk = art_xlate_code_GOTO_BACK;
          relocs = art_xlate_reloc_GOTO_BACK;
        } else {
          chunk = art_xlate_code_GOTO;
          relocs = art_xlate_reloc_GOTO;
        }
        break;
      case Instruction::GOTO_16:
        offset = SignExtend16(inst->VRegA_20t());
        if (offset < 0) {
          chunk = art_xlate_code_GOTO_16_BACK;
          relocs = art_xlate_reloc_GOTO_16_BACK;
        } else {
          chunk = art_xlate_code_GOTO_16;
          relocs = art_xlate_reloc_GOTO_16;
        }
        break;
      case Instruction::GOTO_32:
        offset = inst->VRegA_30t();
        if (offset <= 0) {     // NB: this can branch to itself
          chunk = art_xlate_code_GOTO_32_BACK;
          relocs = art_xlate_reloc_GOTO_32_BACK;
        } else {
          chunk = art_xlate_code_GOTO_32;
          relocs = art_xlate_reloc_GOTO_32;
        }
        break;

        // NOLINT(whitespace/braces)
      case CASE_IF_XX(IF_EQ);
      case CASE_IF_XX(IF_NE);
      case CASE_IF_XX(IF_LT);
      case CASE_IF_XX(IF_GT);
      case CASE_IF_XX(IF_LE);
      case CASE_IF_XX(IF_GE);

      case CASE_IF_XXZ(IF_EQZ);
      case CASE_IF_XXZ(IF_NEZ);
      case CASE_IF_XXZ(IF_LTZ);
      case CASE_IF_XXZ(IF_GTZ);
      case CASE_IF_XXZ(IF_LEZ);
      case CASE_IF_XXZ(IF_GEZ);

      default:
        LOG(INFO) << "Unhandled branch instruction";
        return false;
    }

    // We have the dexpc and the offset (in dex words) from the instruction to the target
    // We also have the offset map which maps each dex word offset into a program offset.
    int target = dexpc + offset;      // Target dexpc value
    int targetprogpc = offset_map[target];  // Target program pc.

#if DEBUG_LOGS
    LOG(INFO) << "translating branch: dexpc: " << dexpc << ", offset: " << offset;
    LOG(INFO) << "target: " << target << ", program target: " << targetprogpc;
    LOG(INFO) << "ppc: " << ppc;
#endif
    int isize = InstructionSize(opcode);

    // The program offset will always be positive because the code chunk
    // contains the correct (add/sub) instruction to do the calculation.

    offset = targetprogpc - ppc;       // Offset to program index from current program pc.
    offset *= 4;                            // Convert to byte offset

#if DEBUG_LOGS
    LOG(INFO) << "calculated offset: " << offset;
#endif

    // Note that we can't use FindChunk here because we need to deal with the special
    // case.
    int size_in_words = relocs - chunk;

    // Allocate memory for it, copy it in and relocate it.
    memcpy(mem, chunk, size_in_words*sizeof(uint32_t));

    // Now relocate the chunk, handling the kRelocOffset relocations ourselves.
    while (*relocs != 0xffffffff) {
      TranslatorRelocations reloc_code =
          static_cast<TranslatorRelocations>(*relocs & 0xffff);  // Relocation code.
      uint16_t reloc_offset = *relocs >> 16;                // Offset into code sequence.

      if (reloc_code == kRelocOffset || reloc_code == kRelocOffset+kRelocLo ||
          reloc_code == kRelocOffset+kRelocHi) {
        // Offset relocation, we handle this ourselves.
        if (IsBranch(mem[reloc_offset])) {
          int32_t realpc = reinterpret_cast<int32_t>(&mem[reloc_offset]) + 8;
          int32_t realtarget = reinterpret_cast<int32_t>(index[targetprogpc]);
          int32_t realoffset = realtarget - realpc;
          realoffset >>= 2;       // In words.
#if DEBUG_LOGS
          LOG(INFO) << "relocating branch realpc: " << std::hex << realpc << ", realtarget: " << realtarget <<
              ", realoffset: " << std::dec << realoffset;
#endif
          if (!RelocateBranch(&mem[reloc_offset], realoffset)) {
            return false;
          }
        } else {
          LOG(INFO) << "Cannot apply reloc_offset to a non branch instruction";
          return false;
        }
      } else {
        // Other relocation, general.
        if (!RelocateOne(inst, mem, *relocs, dexpc)) {
          return false;
        }
      }
      relocs++;
    }

    // Move to next instruction location.
    mem += size_in_words;

    pc += isize;
    ++ppc;
    dexpc += isize;
  } else if (inst->IsSwitch()) {
    // Switch statement.
    if (opcode == Instruction::Code::PACKED_SWITCH) {
      // Packed switch.
      return TranslatePackedSwitch(inst, pc, ppc, dexpc, offset_map, mem, index);
    } else if (opcode == Instruction::Code::SPARSE_SWITCH) {
      // Sparse switch.
      return TranslateSparseSwitch(inst, pc, ppc, dexpc, offset_map, mem, index);
    } else {
      LOG(INFO) << "Impossible happened, unknown switch opcode " << opcode;
      return false;
    }
  } else if (opcode == Instruction::FILL_ARRAY_DATA) {
    return TranslateFillArrayData(inst, pc, ppc, dexpc, offset_map, mem);
  } else if (opcode >= Instruction::ADD_INT_LIT16 && opcode <= Instruction::USHR_INT_LIT8) {
    return TranslateLiteral(inst, pc, ppc, dexpc, offset_map, mem);
  } else {
    uint32_t *chunk = xlator_table[opcode];
    uint32_t *relocs = xlator_reloc_table[opcode];
    switch (opcode) {
      case Instruction::DIV_INT:
        if (false) {
          chunk = art_xlate_code_DIV_INT_SLOW;
        }
        break;
      case Instruction::DIV_INT_2ADDR:
        if (false) {
          chunk = art_xlate_code_DIV_INT_SLOW_2ADDR;
        }
        break;
      case Instruction::REM_INT:
        if (false) {
          chunk = art_xlate_code_REM_INT_SLOW;
        }
        break;
      case Instruction::REM_INT_2ADDR:
        if (false) {
          chunk = art_xlate_code_REM_INT_SLOW_2ADDR;
        }
        break;
      case Instruction::CONST_4: {
        int val = SignExtend4(inst->VRegB_11n(inst_data));
        if (val == 0) {
           chunk = art_xlate_code_CONST_4_ZERO;
           relocs = art_xlate_reloc_CONST_4_ZERO;
        } else if (val < 0) {
          chunk = art_xlate_code_CONST_4_NEGATIVE;
          relocs = art_xlate_reloc_CONST_4_NEGATIVE;
        }
        break;
      }
      case Instruction::CONST_16: {
        int val = SignExtend16(inst->VRegB_21s());
        if (val == 0) {
           chunk = art_xlate_code_CONST_16_ZERO;
           relocs = art_xlate_reloc_CONST_16_ZERO;
        } else if (val < 0) {
          chunk = art_xlate_code_CONST_16_NEGATIVE;
          relocs = art_xlate_reloc_CONST_16_NEGATIVE;
       }
       break;
      }
      default:
        LOG(INFO) << "Unhandled special dex instruction";
        return false;
    }

    int isize = InstructionSize(opcode);
    if (!AddChunk(inst, pc, chunk, relocs, isize, mem, dexpc)) {
      return false;
    }

    pc += isize;
    ++ppc;
    dexpc += isize;
  }
  return true;
}



// This is complicated by the use of negative numbers in the literal instructions.  There
// are both positive and negative versions of the literal instructions.
bool ARMTranslator::TranslateLiteral(const Instruction* inst, const uint16_t*& pc,  int& ppc,
                                       int& dexpc,
                                uint32_t* offset_map, uint32_t*& mem) {
  Instruction::Code opcode = inst->Opcode();
  int index = opcode - Instruction::ADD_INT_LIT16;
  uint32_t *chunk = xlator_table[opcode];
  uint32_t *relocs = xlator_reloc_table[opcode];

#if DEBUG_LOGS
  LOG(INFO) << "translating literal";
#endif
  int val = 0;
  if (opcode >= Instruction::ADD_INT_LIT8) {
    val = SignExtend8(inst->VRegC_22b());
  } else {
    val = SignExtend16(inst->VRegC_22s());
  }
#if DEBUG_LOGS
  LOG(INFO) << "literal value is " << val;
#endif
  // TODO: handle the SLOW DIV and REM variants.
  if (val < 0) {
    chunk = negative_opcodes_code[index];
    relocs = negative_opcodes_relocs[index];
    CHECK(chunk != nullptr);
    CHECK(relocs != nullptr);
  }

  int isize = InstructionSize(opcode);
  if (!AddChunk(inst, pc, chunk, relocs, isize, mem, dexpc)) {
    return false;
  }

  pc += isize;
  ++ppc;
  dexpc += isize;
  return true;
}


bool ARMTranslator::TranslatePackedSwitch(const Instruction* inst, const uint16_t*& pc, int& ppc,
                                       int& dexpc,
                                       uint32_t* offset_map, uint32_t*& mem, uint32_t** index) {
Instruction::Code opcode = inst->Opcode();
  uint32_t *chunk = xlator_table[opcode];
  uint32_t *relocs = xlator_reloc_table[opcode];
  int size_in_words = relocs - chunk;

  // Allocate memory for it, copy it in and relocate it.
  memcpy(mem, chunk, size_in_words*sizeof(uint32_t));

  // Find the packed switch data.
  const uint16_t* switch_data = reinterpret_cast<const uint16_t*>(inst) + inst->VRegB_31t();
  DCHECK_EQ(switch_data[0], static_cast<uint16_t>(Instruction::kPackedSwitchSignature));
  uint16_t size = switch_data[1];
  DCHECK_GT(size, 0);
  const int32_t* keys = reinterpret_cast<const int32_t*>(&switch_data[2]);
  DCHECK(IsAligned<4>(keys));
  int32_t first_key = keys[0];
  const int32_t* targets = reinterpret_cast<const int32_t*>(&switch_data[4]);
  int reloc_index = 0;

  // We need to generate a table of addresses to the code for the packed switch data
  // table.  Each entry in the data is a 32 bit offset from the switch instruction itself
  // (the offset being in Dex word size quantities - 16 bits).  So the first instruction
  // after the switch statement is at offset 3.
  // Each table entry consists of the target address and the target dexpc (8 bytes).
  // We need to keep track of the dex pc for exception handling.
  uint32_t* table = AllocateChunkMemory(size);
  uint32_t branch_table = reinterpret_cast<uint32_t>(table);    // Address of branch table
  for (int i = 0; i < size; i++) {
    int32_t offset = targets[i];
    int target = dexpc + offset;      // Target dexpc value
    int targetprogpc = offset_map[target];  // Target program pc.
#if DEBUG_LOGS
    LOG(INFO) << "translating switch: dexpc: " << dexpc << ", offset: " << offset;
    LOG(INFO) << "target: " << target << ", program target: " << targetprogpc;
#endif
    table[i] = reinterpret_cast<uint32_t>(index[targetprogpc]);
  }

  while (*relocs != 0xffffffff) {
    TranslatorRelocations reloc_code =
        static_cast<TranslatorRelocations>(*relocs & 0xffff);  // Relocation code.
    uint16_t reloc_offset = *relocs >> 16;                // Offset into code sequence.

    if (reloc_code == kRelocConstSpecial) {
      int value = 0;
      switch (reloc_index++) {
        case 0: value = first_key & 0xffff; break;
        case 1: value = (first_key >> 16) & 0xffff; break;
        case 2: value = size & 0xffff; break;
        case 3: value = (size >> 16) & 0xffff; break;
        case 4: value = branch_table & 0xffff; break;
        case 5: value = (branch_table >> 16) & 0xffff; break;
        default:
          LOG(INFO) << "Invalid packed switch relocation";
          return false;
      }
      // Special relocation, we handle this ourselves.
      if (!BitBlat(&mem[reloc_offset], value)) {
        return false;
      }
    } else {
      // Other relocation, general.
      if (!RelocateOne(inst, mem, *relocs, dexpc)) {
        return false;
      }
    }
    relocs++;
  }
  mem += size_in_words;

  pc += 3;
  dexpc += 3;
  ppc += 1;
  return true;
}

bool ARMTranslator::TranslateSparseSwitch(const Instruction* inst, const uint16_t*& pc, int& ppc,
                                       int& dexpc,
                                       uint32_t* offset_map, uint32_t*& mem, uint32_t** index) {
  Instruction::Code opcode = inst->Opcode();
  uint32_t *chunk = xlator_table[opcode];
  uint32_t *relocs = xlator_reloc_table[opcode];
  int size_in_words = relocs - chunk;

  // Allocate memory for it, copy it in and relocate it.
  memcpy(mem, chunk, size_in_words*sizeof(uint32_t));

  // Find the sparse switch data.
  const uint16_t* switch_data = reinterpret_cast<const uint16_t*>(inst) + inst->VRegB_31t();
  DCHECK_EQ(switch_data[0], static_cast<uint16_t>(Instruction::kSparseSwitchSignature));
  uint16_t size = switch_data[1];
  DCHECK_GT(size, 0);
  const int32_t* keys = reinterpret_cast<const int32_t*>(&switch_data[2]);
  DCHECK(IsAligned<4>(keys));
  const int32_t* entries = keys + size;
  int reloc_index = 0;

  // We need to generate a table of addresses to the code for the sparse switch data
  // table.  Each entry in the data is a 32 bit offset from the switch instruction itself
  // (the offset being in Dex word size quantities - 16 bits).  So the first instruction
  // after the switch statement is at offset 3.
  uint32_t* table = AllocateChunkMemory(size);    // TODO: straight malloc?
  uint32_t branch_table = reinterpret_cast<uint32_t>(table);    // Address of branch table
  for (int i = 0; i < size; i++) {
    int32_t offset = entries[i];
    int target = dexpc + offset;      // Target dexpc value
    int targetprogpc = offset_map[target];  // Target program pc.
    table[i] = reinterpret_cast<uint32_t>(index[targetprogpc]);
#if DEBUG_LOGS
    LOG(INFO) << "translating switch: dexpc: " << dexpc << ", offset: " << offset;
    LOG(INFO) << "target: " << target << ", program target: " << targetprogpc;
    LOG(INFO) << "real target: " << std::hex << table[i*2] << std::dec;
#endif
  }

  while (*relocs != 0xffffffff) {
    TranslatorRelocations reloc_code =
        static_cast<TranslatorRelocations>(*relocs & 0xffff);  // Relocation code.
    uint16_t reloc_offset = *relocs >> 16;                // Offset into code sequence.

    if (reloc_code == kRelocConstSpecial) {
      int value = 0;
      switch (reloc_index++) {
        case 0: value = size & 0xffff; break;
        case 1: value = (size >> 16) & 0xffff; break;
        case 2: value = reinterpret_cast<int32_t>(keys) & 0xffff; break;
        case 3: value = (reinterpret_cast<int32_t>(keys) >> 16) & 0xffff; break;
        case 4: value = branch_table & 0xffff; break;
        case 5: value = (branch_table >> 16) & 0xffff; break;
        default:
          LOG(INFO) << "Invalid sparse switch relocation";
          return false;
      }
      // Special relocation, we handle this ourselves.
      if (!BitBlat(&mem[reloc_offset], value)) {
        return false;
      }
    } else {
      // Other relocation, general.
      if (!RelocateOne(inst, mem, *relocs, dexpc)) {
        return false;
      }
    }
    relocs++;
  }
  mem += size_in_words;


  pc += 3;
  dexpc += 3;
  ppc += 1;
  return true;
}

bool ARMTranslator::TranslateFillArrayData(const Instruction* inst, const uint16_t*& pc, int& ppc,
                                       int& dexpc,
                                       uint32_t* offset_map, uint32_t*& mem) {
  Instruction::Code opcode = inst->Opcode();
  uint32_t *chunk = xlator_table[opcode];
  uint32_t *relocs = xlator_reloc_table[opcode];
  int size_in_words = relocs - chunk;

  memcpy(mem, chunk, size_in_words*sizeof(uint32_t));

  // Find the array data.
  const uint16_t* payload_addr = reinterpret_cast<const uint16_t*>(inst) + inst->VRegB_31t();

  int reloc_index = 0;

  while (*relocs != 0xffffffff) {
    TranslatorRelocations reloc_code =
        static_cast<TranslatorRelocations>(*relocs & 0xffff);  // Relocation code.
    uint16_t reloc_offset = *relocs >> 16;                // Offset into code sequence.

    if (reloc_code == kRelocConstSpecial) {
      int value = 0;
      switch (reloc_index++) {
        case 0: value = reinterpret_cast<int32_t>(payload_addr) & 0xffff; break;
        case 1: value = (reinterpret_cast<int32_t>(payload_addr) >> 16) & 0xffff; break;
       default:
          LOG(INFO) << "Invalid fill array data relocation";
          return false;
      }
      // Special relocation, we handle this ourselves.
      if (!BitBlat(&mem[reloc_offset], value)) {
        return false;
      }
    } else {
      // Other relocation, general.
      if (!RelocateOne(inst, mem, *relocs, dexpc)) {
        return false;
      }
    }
    relocs++;
  }

  mem += size_in_words;

  pc += 3;
  dexpc += 3;
  ppc += 1;
  return true;
}





bool ARMTranslator::AddChunk(const Instruction* inst, const uint16_t* pc,
                               uint32_t* chunk, uint32_t* relocs, int isize, uint32_t*& mem,
                               int dexpc) {
  // LOG(INFO) << "dex instruction " << inst->DumpString(nullptr);
  // LOG(INFO) << HexDump(pc, isize*2, true, "chunk ");

  int size_in_words = relocs - chunk;
  memcpy(mem, chunk, size_in_words*sizeof(uint32_t));
#if DEBUG_LOGS
  LOG(INFO) << "adding chunk at address " << mem << " with size " << size_in_words;
#endif

  // Perform required relocations on the code chunk.
  if (!Relocate(inst, mem, relocs, dexpc)) {
    return false;
  }
  mem += size_in_words;
  return true;
}

uint32_t* Translator::AllocateChunkMemory(int size_in_words) {
  if (UNLIKELY(end_pool_ == nullptr || (end_pool_ + size_in_words) >= end_element_)) {
    // No space in last element or no pool at all.
#if DEBUG_LOGS
    LOG(INFO) << "No memory in pool, allocating some";
#endif
    end_pool_ = static_cast<uint32_t*>(malloc(kPoolElementSize*sizeof(uint32_t*)));
    pool_.push_back(end_pool_);
    end_element_ = end_pool_ + kPoolElementSize;

    MakeExecutable(end_pool_, kPoolElementSize*sizeof(uint32_t*));
  }

  // There is space in the pool, bump the pointer.
  uint32_t* mem = end_pool_;
  end_pool_ += size_in_words;
  return mem;
}

// Add a shared chunk to the program.  A shared chunk is one that can be shared among various
// methods.  It is called using a BL instruction.
bool ARMTranslator::AddSharedChunk(const Instruction* inst, const uint16_t* pc,
                               uint32_t* chunk, uint32_t* relocs, int isize, uint32_t*& mem,
                               int dexpc) {
  MutexLock mu(Thread::Current(), lock_);

  int size_in_words = relocs - chunk;
  LOG(INFO) << "Adding shared chunk of " << size_in_words << " words";
  uint32_t* chunkmem = chunk_table_.find(pc, isize);
  if (chunkmem == nullptr) {
#if DEBUG_LOGS
    LOG(INFO) << "no chunk found, allocating one for " << size_in_words << " words";
#endif
    // Allocate memory for it, copy it in and relocate it
    chunkmem = AllocateChunkMemory(size_in_words);
    memcpy(chunkmem, chunk, size_in_words*sizeof(uint32_t));

    // Add the allocated chunk to the chunk table for next time.
    chunk_table_.add(pc, isize, chunkmem);

    // Perform required relocations on the code chunk
    if (!Relocate(inst, mem, relocs, dexpc)) {
      return false;
    }

    MakeExecutable(chunkmem, size_in_words*sizeof(uint32_t));
  }

  // Add a branch instruction to the code sequence.
  uint32_t bl_instruction = 0xeb000000;
  int32_t offset_in_bytes = reinterpret_cast<int32_t>(chunkmem) -
      (reinterpret_cast<int32_t>(mem) + 8);
  int32_t offset_in_words = offset_in_bytes >> 2;
  bl_instruction |= (offset_in_words & 0xffffff);
  // LOG(INFO) << "branch offset: " << offset_in_words;
  *mem = bl_instruction;
  mem += 1;           // One instruction.
  return true;
}

bool Translator::IsSpecial(const Instruction* inst) {
  // Branches and switches require special handling.
  if (inst->IsBranch() || inst->IsSwitch()) {
    return true;
  }
  Instruction::Code opcode = inst->Opcode();

  if (opcode == static_cast<Instruction::Code>(Instruction::kArrayDataSignature)) {
    return true;
  }

  // Literals are special (they have negative versions).
  if (opcode >= Instruction::ADD_INT_LIT16 && opcode <= Instruction::USHR_INT_LIT8) {
    return true;
  }

  // TODO: Divide and Remainder instructions depend on CPU features.
  switch (opcode) {
    case Instruction::DIV_INT:
    case Instruction::DIV_INT_2ADDR:
    case Instruction::REM_INT:
    case Instruction::REM_INT_2ADDR:
    case Instruction::CONST_4:
    case Instruction::CONST_16:
    case Instruction::FILL_ARRAY_DATA:
      return true;
    default:
      return false;
  }
  return false;
}


// Relocations are two half words.  The first is the relocation code, the second
// is the offset into the codemem (in words) to relocate.
bool Translator::Relocate(const Instruction* inst, uint32_t *codemem, uint32_t *relocs, int dexpc) {
  while (*relocs != 0xffffffff) {
    if (!RelocateOne(inst, codemem, *relocs, dexpc)) {
      return false;
    }
    relocs++;
  }
  return true;
}

// Perform a single relocation.
bool ARMTranslator::RelocateOne(const Instruction* inst, uint32_t* codemem, uint32_t reloc, int dexpc) {
#if DEBUG_LOGS
  LOG(INFO) << "Relocating instruction " << inst->DumpString(nullptr);
#endif
  TranslatorRelocations reloc_code =
       static_cast<TranslatorRelocations>(reloc & 0xffff);  // Relocation code.
  uint16_t reloc_offset = reloc >> 16;                // Offset into code sequence.
  CHECK_LT(reloc_offset, 1000);                       // Catch missing relocation labels.

#if DEBUG_LOGS
  LOG(INFO) << "relocation code: " << reloc_code << ", offset: " << reloc_offset;
#endif
  // Extract the DEX instruction.
  uint16_t inst_data = inst->Fetch16(0);

  // 32 bit value to insert into ARM instruction.
  uint32_t value = 0;

  int isize = InstructionSize(inst->Opcode());
  int reloc_op = reloc_code & ~(kRelocGap-1);     // Top bits are the operation.
  int reloc_mod = reloc_code & (kRelocGap-1);    // Bottom bits are the mode.

  if (reloc_code == kRelocInstruction) {
    // Write address of dex instruction into the address given in the relocation.
    codemem[reloc_offset]  = reinterpret_cast<uint32_t>(inst);
    return true;
  } else if (reloc_code == kRelocDexSize) {
    value = isize;
  } else if (reloc_code >= kRelocDexPC && reloc_code < (kRelocDexPC+kRelocGap)) {
    codemem[reloc_offset] = ApplyRelocModifier(dexpc, reloc_mod);
    return true;
  } else if (reloc_code == kRelocHelperAddr) {
    // Relocation of a call to a helper.  Helper number is the value of the instruction
    // and is an offset into the art_xlator_helpers array.
    // The field contains the instruction encoding for a BL instruction (with condition code)
    // in the upper 8 bits.
    int helper = codemem[reloc_offset] & 0xffffff;                // Lower 23 bits.
    uint32_t instruction = codemem[reloc_offset] & ~0xffffff;     // Upper 8 bits.

    // LOG(INFO) << "relocating helper " << helper;
    uint32_t realpc = reinterpret_cast<uint32_t>(&codemem[reloc_offset]) + 8;   // current pc value

    uint32_t helperaddr = FindHelperTrampoline(realpc, helper);
    if (helperaddr == 0) {
      // This should not happen since we've already allocated the trampoline if we need to.
      LOG(INFO) << "Failed to get trampoline for branch instruction";
      return false;     // And we can't translate this method.
    }

    int32_t offset = helperaddr - realpc;
    offset >>= 2;     // In words.

    // LOG(INFO) << "offset is " << offset << ", realpc: " << std::hex << realpc << ", helperaddr: " <<
       // helperaddr << std::dec;

    // Form a BL instruction in A1 encoding and write it in
    instruction |= offset & 0xffffff;
    codemem[reloc_offset] = instruction;
    // LOG(INFO) << "bl instruction is " << std::hex << instruction << std::dec;
    return true;
  } else if (reloc_code >= kRelocConst && reloc_code < kRelocEndConsts) {
    // Constant relocation.
    switch (reloc_op) {
      case kRelocConstB_11n: value = ApplyRelocModifier(inst->VRegB_11n(inst_data), reloc_mod); break;
      case kRelocConstB_21s: value = ApplyRelocModifier(inst->VRegB_21s(), reloc_mod); break;
      case kRelocConstB_21h: value = ApplyRelocModifier(inst->VRegB_21h(), reloc_mod); break;
      case kRelocConstB_51l: value = ApplyRelocModifier(inst->VRegB_51l(), reloc_mod); break;
      case kRelocConstB_51l_2: value = ApplyModifierHighWord(inst->VRegB_51l(), reloc_mod);
        break;
      case kRelocConstB_21t: value = ApplyRelocModifier(inst->VRegB_21t(), reloc_mod); break;
      case kRelocConstB_31i: value = ApplyRelocModifier(inst->VRegB_31i(), reloc_mod); break;
      case kRelocConstB_21c: value = ApplyRelocModifier(inst->VRegB_21c(), reloc_mod); break;
      case kRelocConstB_31c: value = ApplyRelocModifier(inst->VRegB_31c(), reloc_mod); break;
      case kRelocConstC_22t: value = ApplyRelocModifier(inst->VRegC_22t(), reloc_mod); break;
      case kRelocConstC_22b: value = ApplyRelocModifier(inst->VRegC_22b(), reloc_mod); break;
      case kRelocConstC_22s: value = ApplyRelocModifier(inst->VRegC_22s(), reloc_mod); break;
     default:
        LOG(FATAL) << "Unknown translator constant relocation " << reloc_code;
    }
  } else {
    switch (reloc_op) {
      // VregA relocations.
      case kRelocVregA_10t: value = ApplyRelocModifier(inst->VRegA_10t(inst_data), reloc_mod); break;
      case kRelocVregA_10x: value = ApplyRelocModifier(inst->VRegA_10x(inst_data), reloc_mod); break;
      case kRelocVregA_11n: value = ApplyRelocModifier(inst->VRegA_11n(inst_data), reloc_mod); break;
      case kRelocVregA_11x: value = ApplyRelocModifier(inst->VRegA_11x(inst_data), reloc_mod); break;
      case kRelocVregA_12x: value = ApplyRelocModifier(inst->VRegA_12x(inst_data), reloc_mod); break;
      case kRelocVregA_20t: value = ApplyRelocModifier(inst->VRegA_20t(), reloc_mod); break;
      case kRelocVregA_21c: value = ApplyRelocModifier(inst->VRegA_21c(inst_data), reloc_mod); break;
      case kRelocVregA_21h: value = ApplyRelocModifier(inst->VRegA_21h(inst_data), reloc_mod); break;
      case kRelocVregA_21s: value = ApplyRelocModifier(inst->VRegA_21s(inst_data), reloc_mod); break;
      case kRelocVregA_21t: value = ApplyRelocModifier(inst->VRegA_21t(inst_data), reloc_mod); break;
      case kRelocVregA_22b: value = ApplyRelocModifier(inst->VRegA_22b(inst_data), reloc_mod); break;
      case kRelocVregA_22c: value = ApplyRelocModifier(inst->VRegA_22c(inst_data), reloc_mod); break;
      case kRelocVregA_22s: value = ApplyRelocModifier(inst->VRegA_22s(inst_data), reloc_mod); break;
      case kRelocVregA_22t: value = ApplyRelocModifier(inst->VRegA_22t(inst_data), reloc_mod); break;
      case kRelocVregA_22x: value = ApplyRelocModifier(inst->VRegA_22x(inst_data), reloc_mod); break;
      case kRelocVregA_23x: value = ApplyRelocModifier(inst->VRegA_23x(inst_data), reloc_mod); break;
      case kRelocVregA_30t: value = ApplyRelocModifier(inst->VRegA_30t(), reloc_mod); break;
      case kRelocVregA_31c: value = ApplyRelocModifier(inst->VRegA_31c(inst_data), reloc_mod); break;
      case kRelocVregA_31i: value = ApplyRelocModifier(inst->VRegA_31i(inst_data), reloc_mod); break;
      case kRelocVregA_31t: value = ApplyRelocModifier(inst->VRegA_31t(inst_data), reloc_mod); break;
      case kRelocVregA_32x: value = ApplyRelocModifier(inst->VRegA_32x(), reloc_mod); break;
      case kRelocVregA_35c: value = ApplyRelocModifier(inst->VRegA_35c(inst_data), reloc_mod); break;
      case kRelocVregA_3rc: value = ApplyRelocModifier(inst->VRegA_3rc(inst_data), reloc_mod); break;
      case kRelocVregA_51l: value = ApplyRelocModifier(inst->VRegA_51l(inst_data), reloc_mod); break;

      // VregB relocations.
      case kRelocVregB_11n: value = ApplyRelocModifier(inst->VRegB_11n(inst_data), reloc_mod); break;
      case kRelocVregB_12x: value = ApplyRelocModifier(inst->VRegB_12x(inst_data), reloc_mod); break;
      case kRelocVregB_21c: value = ApplyRelocModifier(inst->VRegB_21c(), reloc_mod); break;
      case kRelocVregB_21h: value = ApplyRelocModifier(inst->VRegB_21h(), reloc_mod); break;
      case kRelocVregB_21s: value = ApplyRelocModifier(inst->VRegB_21s(), reloc_mod); break;
      case kRelocVregB_21t: value = ApplyRelocModifier(inst->VRegB_21t(), reloc_mod); break;
      case kRelocVregB_22b: value = ApplyRelocModifier(inst->VRegB_22b(), reloc_mod); break;
      case kRelocVregB_22c: value = ApplyRelocModifier(inst->VRegB_22c(inst_data), reloc_mod); break;
      case kRelocVregB_22s: value = ApplyRelocModifier(inst->VRegB_22s(inst_data), reloc_mod); break;
      case kRelocVregB_22t: value = ApplyRelocModifier(inst->VRegB_22t(inst_data), reloc_mod); break;
      case kRelocVregB_22x: value = ApplyRelocModifier(inst->VRegB_22x(), reloc_mod); break;
      case kRelocVregB_23x: value = ApplyRelocModifier(inst->VRegB_23x(), reloc_mod); break;
      case kRelocVregB_31c: value = ApplyRelocModifier(inst->VRegB_31c(), reloc_mod); break;
      case kRelocVregB_31i: value = ApplyRelocModifier(inst->VRegB_31i(), reloc_mod); break;
      case kRelocVregB_31t: value = ApplyRelocModifier(inst->VRegB_31t(), reloc_mod); break;
      case kRelocVregB_32x: value = ApplyRelocModifier(inst->VRegB_32x(), reloc_mod); break;
      case kRelocVregB_35c: value = ApplyRelocModifier(inst->VRegB_35c(), reloc_mod); break;
      case kRelocVregB_3rc: value = ApplyRelocModifier(inst->VRegB_3rc(), reloc_mod); break;
      case kRelocVregB_51l: value = ApplyRelocModifier(inst->VRegB_51l(), reloc_mod); break;

      // VregC relocations.
      case kRelocVregC_22b: value = ApplyRelocModifier(inst->VRegC_22b(), reloc_mod); break;
      case kRelocVregC_22c: value = ApplyRelocModifier(inst->VRegC_22c(), reloc_mod); break;
      case kRelocVregC_22s: value = ApplyRelocModifier(inst->VRegC_22s(), reloc_mod); break;
      case kRelocVregC_22t: value = ApplyRelocModifier(inst->VRegC_22t(), reloc_mod); break;
      case kRelocVregC_23x: value = ApplyRelocModifier(inst->VRegC_23x(), reloc_mod); break;
      case kRelocVregC_35c: value = ApplyRelocModifier(inst->VRegC_35c(), reloc_mod); break;
      case kRelocVregC_3rc: value = ApplyRelocModifier(inst->VRegC_3rc(), reloc_mod); break;
      default:
        LOG(INFO) << "Unknown translator relocation " << reloc_code;
        return false;
    }
  }

  // 'value' will contain the bits to insert into the instruction.
  return BitBlat(&codemem[reloc_offset], value, reloc_mod);
}

// Relocate a branch instruction.
bool ARMTranslator::RelocateBranch(uint32_t* instptr, int offset) {
  *instptr &= ~0xffffff;       // Clear imm24 in instruction.
  *instptr |= offset & 0xffffff;   // Add in offset.
#if DEBUG_LOGS
  LOG(INFO) << "relocated branch instruction: " << std::hex << *instptr << std::dec;
#endif
  return true;
}


// Given a pointer to an ARM instruction and a value to insert into it,
// perform a read-modify-write operation on the instruction.  This
// works out the instruction type and encoding before firing the
// bits into it.
bool ARMTranslator::BitBlat(uint32_t* code, int value, int modifier) {
#if DEBUG_LOGS
  LOG(INFO) << "bitblatting code with value " << std::hex << value << std::dec;
#endif

  uint32_t arminst = *code;
  if (IsLoadStore(arminst)) {
#if DEBUG_LOGS
    LOG(INFO) << "LDR/STR instruction";
#endif
    // 12 bit immediate.
    int32_t immed = arminst & 0xfff;
    if (modifier != kRelocRaw) {
      value <<= 2;          // These are used for vreg offset which are 32 bit.
    }
    immed += value & 0xfff;     // Unencoded immediate value for LDR/STR instructions.
    int basereg = GetLoadStoreRn(arminst);
    if (immed == 0 && basereg == 7) {   // Optimize v0
      constexpr uint32_t kMovInst = 0xe1a00000;
      if (IsLoad(arminst)) {
        // Loading from v0 becomes a mov from r11
        int dest = GetLoadStoreRt(arminst);
        arminst = kMovInst | 11 | (dest << 12);
        *code = arminst;
        return true;
      } else {
        // Store into v0 becomes a mov into r11
        int src = GetLoadStoreRt(arminst);
        arminst = kMovInst | (11 << 12) | src;
        *code = arminst;
        return true;
     }
    }
    if (immed == 0 && basereg == 5) {   // Optimize v0 (shadow copy)
      constexpr uint32_t kMovInst = 0xe1a00000;
      if (IsLoad(arminst)) {
        // Loading from v0 becomes a mov from r12
        int dest = GetLoadStoreRt(arminst);
        arminst = kMovInst | 12 | (dest << 12);
        *code = arminst;
        return true;
      } else {
        // Store into v0 becomes a mov into r12
        int src = GetLoadStoreRt(arminst);
        arminst = kMovInst | (12 << 12) | src;
        *code = arminst;
        return true;
     }
    }
    if ((value & ~0xfff) != 0) {
      LOG(INFO) << "LDR/STR offset out of range (only 12 bits allowed): " <<
          std::hex << value << std::dec;
      return false;
    }
    arminst = (arminst & ~0xfff) | (immed & 0xfff);
  } else if (IsStrHalf(arminst)) {
    uint32_t immed = ((arminst & 0b111100000000) >> 4) | (arminst & 0b1111);
    immed += value;
    uint32_t hi = immed & 0b11110000;
    uint32_t lo = immed & 0b1111;
    arminst &= ~0b111100001111;
    arminst |= (hi << 4) | lo;
  } else if (IsVectorLoadStore(arminst)) {
#if DEBUG_LOGS
    LOG(INFO) << "vector load/store";
#endif
    // 10 bit immediate.
    int32_t immed = arminst & 0xff;
    if (modifier != kRelocRaw) {
      value <<= 2;          // These are used for vreg offset which are 32 bit.
    }
    if ((value & ~0x3ff) != 0) {
      LOG(INFO) << "VLDR/VSTR offset out of range (only 10 bits allowed): " <<
          std::hex << value << std::dec;
      return false;
    }
    value >>= 2;                // immediate field is shifted left by 2
    immed += value & 0xff;     // Unencoded immediate value for LDR/STR instructions.
    arminst = (arminst & ~0xff) | (immed & 0xff);
  } else if (IsMov(arminst)) {
    if (IsMovW(arminst)) {
#if DEBUG_LOGS
      LOG(INFO) << "MOVW instruction";
#endif

      // movw (T3 encoding).
      // 16 bit immediate, split into 4 parts.
      uint8_t imm8 = arminst & 0xff;
      uint8_t imm3 = (arminst >> 12) & 0x7;
      uint8_t imm4 = (arminst >> 16) & 0xf;
      uint8_t i = (arminst >> 26) & 1;
      uint16_t imm = imm8 | (imm3 << 8) | (imm4 << 11) | (i << 15);
      imm += value & 0xffff;
      arminst = arminst & ~0b00000100000011110111000011111111;
      arminst |= (imm & 0xff) | (((imm >> 8) & 0x3) << 12)
                          | (((imm >> 11) & 0xf) << 16)
                          | (((imm >> 15) & 1) << 26);
    } else if (IsMovT(arminst)) {
#if DEBUG_LOGS
      LOG(INFO) << "MOVT instruction";
#endif
      if (IsMovTA1(arminst)) {
        // 16 bit immediate, split into 2 parts.
        uint16_t imm12 = arminst & 0xfff;
        uint8_t imm4 = (arminst >> 16) & 0xf;
        uint16_t imm = imm12 | (imm4 << 12);
        imm += value & 0xffff;
        arminst = (arminst & ~0x000f0fff) | (imm & 0xfff) | ((imm >> 12) << 16);
      } else {
        // 16 bit immediate, split into 4 parts.
        uint8_t imm8 = arminst & 0xff;
        uint8_t imm3 = (arminst >> 12) & 0x7;
        uint8_t imm4 = (arminst >> 16) & 0xf;
        uint8_t i = (arminst >> 26) & 1;
        uint16_t imm = imm8 | (imm3 << 8) | (imm4 << 11) | (i << 15);
        imm += value & 0xffff;
        arminst = arminst & ~0b00000100000011110111000011111111;
        arminst |= (imm & 0xff) | (((imm >> 8) & 0x3) << 12)
                  | (((imm >> 11) & 0xf) << 16)
                  | (((imm >> 15) & 1) << 26);
      }
    } else if (IsMovA2(arminst)) {
#if DEBUG_LOGS
      LOG(INFO) << "MOV A2 instruction";
#endif
      // ARM A2 encoded
      // 16 bit immediate, 2 parts.
      uint16_t imm12 = arminst & 0xfff;
      uint8_t imm4 = (arminst >> 16) & 0xf;
      uint16_t imm = imm12 | (imm4 << 16);
      imm += value & 0xffff;
      arminst = (arminst & ~0x000f0fff) | (imm & 0xfff) | ((imm >> 12) << 16);
    } else {
#if DEBUG_LOGS
      LOG(INFO) << "MOV A1 instruction";
#endif
      // ARM A1 encoded
      // 12 bit immediate encoded, easy one.
      uint16_t imm12 = arminst & 0xfff;
      imm12 += EncodedImmediate(value & 0xfff);
      arminst = (arminst & ~0xfff) | (imm12 & 0xfff);
    }
  } else if (IsShift(arminst)) {
#if DEBUG_LOGS
    LOG(INFO) << "Shift instruction";
#endif
    // For a shift, the assembler will not generate the correct instruction if
    // the literal shift is #0.  So we cannot add to the shift in this relocation.
    // shift immediate, A1 encoding.
    // 5 bit encoded immediate.
    // Note: no addition to the original instruction.
    arminst = (arminst & ~0x0f80) | ((value & 0x1f) << 7);
  } else if (IsDataProcessing(arminst)) {
#if DEBUG_LOGS
    LOG(INFO) << "data processing instruction";
#endif
    // ARM A1 encoded
    // 12 bit immediate, encoded
    uint16_t imm12 = arminst & 0xfff;
    imm12 += EncodedImmediate(value & 0xfff);
    arminst = (arminst & ~0xfff) | (imm12 & 0xfff);
  } else {
    LOG(INFO) << "Unknown ARM instruction encoding: 0x" << std::hex << arminst << std::dec;
    return false;
  }

  // Write the instruction back.
#if DEBUG_LOGS
  LOG(INFO) << "new instruction: 0x" << std::hex << arminst << std::dec;
#endif
  *code = arminst;
  return true;
}

#ifdef NDEBUG
class DisassemblerArm {
 public:
  void Dump(std::ostream& os, uint8_t* p) {}
};
#endif

// Look through the program for small opportunistic instruction removals.
uint32_t* ARMTranslator::PeepHole(uint32_t* program, int entrypoint_size_in_words) {
  // The program starts with an array of addresses of translated DEX instructions.
  uint32_t** index = reinterpret_cast<uint32_t**>(program) + entrypoint_size_in_words;

  // We check for peephole optimizations at the junction of these instructions.

  DisassemblerArm dasm;

  uint32_t** end = reinterpret_cast<uint32_t**>(index[0]);
  uint32_t instindex = 1;
  uint32_t endindex = end - index - 1;

  while (instindex < endindex) {
    uint32_t* prevptr = index[instindex-1];
    uint32_t* instptr = index[instindex];
    uint32_t* nextptr = index[instindex+1];
    ++instindex;

    while (instptr < nextptr) {
      // Look at current instruction

      uint32_t inst = *instptr;
      if (IsLoadStore(inst) && IsLoad(inst) && GetLoadStoreRn(inst) == 7) {
        // This is an LDR from r7
        // Look back for a store to the same offset from r7
        uint16_t offset = inst & 0xfff;
        int reg2 = GetLoadStoreRt(inst);
        LOG(INFO) << "LDR r" << reg2 << ", [r7,#" << offset << "]";
        dasm.Dump(LOG(INFO), reinterpret_cast<uint8_t*>(instptr));
        uint32_t* p = instptr - 1;
        while (p > prevptr) {
          uint32_t pinst = *p;
          if (IsLoadStore(pinst)) {
            int reg1 = GetLoadStoreRt(pinst);
            if (IsLoad(pinst)) {
              if (reg1 == reg2) {
                // Load to same register, stop
                LOG(INFO) << "load of same register, stopping backward scan";
                break;
              }
            } else {
              // This is a store
              if (GetLoadStoreRn(pinst) == 7 && (pinst & 0xfff) == offset) {
                LOG(INFO) << "Found load from same reg as store in peephole";
                dasm.Dump(LOG(INFO), reinterpret_cast<uint8_t*>(p));
                // LOG(INFO) << "inst: " << std::hex << inst;
                // LOG(INFO) << "pinst: " << std::hex << pinst;
                constexpr uint32_t kMovInst = 0xe1a00000;
                *instptr = kMovInst | reg1 | (reg2 << 12);
                break;
              }
            }
          } else {
            break;    // stop backward scan at first non load/store.
          }
          --p;
        }
        ++instptr;        // Look at next ARM instruction.
      } else {
        // Stop at first non load
        break;
      }
    }
  }

  return program;
}


ChunkTable::ChunkTable() {
  memset(opcodes, 0, sizeof(opcodes));
}

ChunkTable::~ChunkTable() {
  for (int i = 0 ; i < 256 ; i++) {
    if (opcodes[i] != nullptr) {
      for (int j = 0 ; j < 256; j++) {
        delete opcodes[i][j];
      }
      free(opcodes[i]);
    }
  }
}

void ChunkTable::add(const uint16_t* instr, int len, uint32_t* addr) {
  int opcode = *instr & 0xff;
  int val = *instr >> 8;
  Chunk** opchunk = opcodes[opcode];
  if (opchunk == nullptr) {
    opchunk = opcodes[opcode] = static_cast<Chunk**>(malloc(256*sizeof(Chunk**)));
    memset(opchunk, 0, sizeof(opchunk) * 256);
  }
  Chunk* chunk = opchunk[val];
  if (chunk == nullptr) {
    chunk = new Chunk();
    opchunk[val] = chunk;
  }
  if (len == 1) {
    if (chunk->addr != nullptr) {
      LOG(FATAL) << "Duplicate instruction";
    } else {
      chunk->addr = addr;
    }
  } else {
    chunk->add(&instr[1], len - 1, addr);
  }
}

void Chunk::add(const uint16_t* instr, int len, uint32_t* addr) {
  if (len == 0) {
    if (this->addr != nullptr) {
        LOG(FATAL) << "Duplicate chunk found";
        return;
    }
    this->addr = addr;
    return;
  }
  if (children == nullptr) {
    children = new ChunkMap();
  }
  ChunkMap::iterator i = children->find(*instr);
  Chunk *next = nullptr;
  if (i == children->end()) {
    next = new Chunk();
    (*children)[*instr] = next;
  } else {
    next = i->second;
  }
  next->add(&instr[1], len - 1, addr);
}

uint32_t* ChunkTable::find(const uint16_t* instr, int len) {
  int opcode = *instr & 0xff;
  int val = *instr >> 8;
  Chunk** opchunk = opcodes[opcode];
  if (opchunk == nullptr) {
    return nullptr;
  }
  Chunk* chunk = opchunk[val];
  if (chunk == nullptr) {
    return nullptr;
  }
  if (len == 1) {
     return chunk->addr;
  }
  return chunk->find(&instr[1], len - 1);
}

uint32_t* Chunk::find(const uint16_t* instr, int len) {
  if (len == 0) {
    return addr;
  }
  if (children == nullptr) {
    return nullptr;
  }
  ChunkMap::iterator i = children->find(*instr);
  if (i == children->end()) {
    return nullptr;
  }
  return i->second->find(&instr[1], len - 1);
}


void ChunkTable::print() {
  for (int i = 0; i < 256; i++) {
    if (opcodes[i] != nullptr) {
      Chunk **opchunk = opcodes[i];
      for (int j = 0; j < 256; j++) {
        if (opchunk[j] != nullptr) {
          LOG(INFO) << "[" << i << "][" << j << "]: ";
          opchunk[j]->print();
        }
      }
    }
  }
}

void Chunk::print() {
  if (addr != nullptr) {
    LOG(INFO) << "addr: " << addr << " ";
  }
  if (children != nullptr) {
    for (ChunkMap::iterator i = children->begin() ; i != children->end() ; i++) {
      Chunk *child = i->second;
      uint16_t instr = i->first;
      LOG(INFO) << "child " << std::hex << instr << std::dec << " ";
      child->print();
    }
  }
}


}   // namespace arm
}   // namespace art


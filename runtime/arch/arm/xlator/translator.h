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

#ifndef ART_RUNTIME_ARCH_ARM_XLATOR_TRANSLATOR_H_
#define ART_RUNTIME_ARCH_ARM_XLATOR_TRANSLATOR_H_

#include <stdint.h>
#include <map>
#include "dex_instruction.h"
#include <vector>
#include "base/mutex.h"
#include "mirror/art_method.h"

namespace art {
namespace arm {

extern const char* xlator_format_table[];

class Translator;

struct TranslatedMethod {
  ~TranslatedMethod();

  uint32_t** program_;
  uint32_t dexpc_map_size_;     // Number of dex instructions in map.
  union {
    uint32_t* dexpc_map_;       // Bitmap for larger methods.
    uint32_t small_map_;        // Bitmap for small methods (<= 32 dex instructions).
  };
  uint32_t **end_program_;      // Address of the last machine instruction + 4.

  uint32_t index_size_;         // Size of program index.
  uint8_t* ppc_map_;            // Program PC map.
  Translator* translator_;      // The translator object.
  TranslatedMethod* next_;      // Next in cache list.
  TranslatedMethod* prev_;      // Previous in cahce list.
  uint32_t program_size_;       // Size of program in bytes.
  mirror::ArtMethod* method_;   // Original ART method.
  uint32_t entry_count_;        // Number of current activations or this method (atomic).
};

constexpr uint32_t kAttrInline = 1;
constexpr uint32_t kAttrShared = 2;

extern "C" uint32_t** TranslateDexPC(TranslatedMethod* meth, uint32_t dexpc);
extern "C" uint32_t LookupDexPC(TranslatedMethod* meth, uint32_t pc);

// This is a table of chunks of relocated code representing one
// binary value for a dex instruction.  The first level is an
// array of 256 pointers to arrays of 256 pointers.  The opcode
// for the dex instruction (the lower 8 bits) are used to index
// the first array, giving a pointer to another array for the
// high byte.  The subarrays (representing the full 16 bits of the
// first word of a dex instruction) contain pointers to a map
// of Chunks.  Each chunk map entry contains the 16 bit dex instruction
// word vs a struct containing a possible chunk pointer and a pointer
// to a map for further instruction words.
struct Chunk;
typedef std::map<const uint16_t, Chunk*> ChunkMap;

struct Chunk {
  Chunk() : addr(nullptr), children(nullptr) {}
  explicit Chunk(uint32_t* addr) : addr(addr), children(nullptr) {}
  explicit Chunk(ChunkMap* cmap) : addr(nullptr), children(cmap) {}
  ~Chunk() {
    delete children;
  }

  void add(const uint16_t* instr, int len, uint32_t* addr);
  uint32_t *find(const uint16_t* instr, int len);

  void print();
  uint32_t* addr;
  ChunkMap* children;
};

class ChunkTable {
 public:
  explicit ChunkTable();
  ~ChunkTable();

  void add(const uint16_t* instr, int len, uint32_t* addr);
  uint32_t* find(const uint16_t* addr, int len);

  void print();
 private:
  Chunk** opcodes[256];

  // Given a pointer to an instruction word, the length of the
  // instruction in words, and a current position, find the Chunk
  // if it exists, nullptr otherwise.
  Chunk* findChunk(const uint16_t *instr, int len, int pos);
};

// TODO: move this to cross-platform location.
class Translator {
  friend struct TranslatedMethod;
 public:
  explicit Translator(ChunkTable& chunk_table, const uint32_t helper_size);
  virtual ~Translator() {
    for (auto &i : pool_) {
      free(i);
    }
  }

  mirror::EntryPointFromInterpreter* Translate(mirror::ArtMethod* const method,
                                                       const uint16_t* code,
                                                       const uint16_t* endcode);

  void Clear();
  void ShowCacheSize();

 protected:
  virtual bool TranslateSpecial(const Instruction* inst, const uint16_t*& pc,  int& ppc, int& dexpc,
                             uint32_t* offset_map, uint32_t*& mem, uint32_t** index) = 0;
  virtual bool TranslatePackedSwitch(const Instruction* inst, const uint16_t*& pc,  int& ppc, int& dexpc,
                                  uint32_t* offset_map, uint32_t*& mem, uint32_t** index) = 0;
  virtual bool TranslateSparseSwitch(const Instruction* inst, const uint16_t*& pc,  int& ppc, int& dexpc,
                                  uint32_t* offset_map, uint32_t*& mem, uint32_t** index) = 0;
  virtual bool TranslateFillArrayData(const Instruction* inst, const uint16_t*& pc,  int& ppc, int& dexpc,
                                  uint32_t* offset_map, uint32_t*& mem) = 0;
  virtual bool TranslateLiteral(const Instruction* inst, const uint16_t*& pc,  int& ppc, int& dexpc,
                                  uint32_t* offset_map, uint32_t*& mem) = 0;

  virtual int SizeofSpecial(const Instruction* inst) = 0;

  bool IsSpecial(const Instruction* inst);

  void MakeExecutable(void* addr, uint32_t len);

  bool IsData(uint16_t inst) {
    // packed-switch, sparse-switch or filled-array-data payloads.
    return inst == 0x100 || inst == 0x200 || inst == 0x300;
  }

  int InstructionSize(uint8_t opcode) {
    const char *format = xlator_format_table[opcode];
     // The format is a string like k21s.  The second character is the number of bytes.
     return format[1] - '0';
  }

  bool Relocate(const Instruction* inst, uint32_t *codemem, uint32_t *relocs, int dexpc);

  virtual bool RelocateOne(const Instruction* inst, uint32_t* code, uint32_t reloc, int decpc) = 0;

  virtual bool AddChunk(const Instruction* inst, const uint16_t* pc,
                      uint32_t* chunk, uint32_t* relocs, int isize, uint32_t*& mem, int dexpc) = 0;

  virtual bool AddSharedChunk(const Instruction* inst, const uint16_t* pc,
                      uint32_t* chunk, uint32_t* relocs, int isize, uint32_t*& mem, int dexpc) = 0;


  virtual bool BitBlat(uint32_t *code, int value, int modifier = 0) = 0;

  virtual uint32_t* PeepHole(uint32_t* program, int entrypoint_size_in_words) = 0;

  uint32_t* AllocateChunkMemory(int size_in_words);

  // Table holding chunks of code indexed by the binary encoding of an instruction instance.
  ChunkTable& chunk_table_;

  // Table for memory for the location of the chunks.
  static constexpr int kPoolElementSize = 4096;     // In words.
  typedef std::vector<uint32_t*> PoolElements;
  PoolElements pool_;
  uint32_t* end_element_;   // End of current pool element.
  uint32_t* end_pool_;      // Last unused location in last pool element.

  Mutex lock_;

  uint32_t cache_size_;
  static constexpr uint32_t kCacheSizeInBytes = 128 * 1024;

  TranslatedMethod* cache_head_;      // Head of cache list
  TranslatedMethod* cache_tail_;      // Last element in cache list

  uint32_t helper_size_;
  std::vector<uint32_t*> helper_trampolines_;

  virtual void AllocateHelperTrampoline(uint32_t* addr)=0;
  virtual uint32_t FindHelperTrampoline(uint32_t pc, uint32_t helper) = 0;

  void DeleteMethod(TranslatedMethod* method);
  void CacheMethod(TranslatedMethod* method) LOCKS_EXCLUDED(lock_);
  bool MakeRoomInCache(uint32_t sizeneeded) LOCKS_EXCLUDED(lock_);
};

class ARMTranslator : public Translator {
 public:
  explicit ARMTranslator(ChunkTable& chunk_table, const uint32_t helper_size);

 private:
  bool TranslateSpecial(const Instruction* inst, const uint16_t*& pc,  int& ppc, int& dexpc,
                             uint32_t* offset_map, uint32_t*& mem, uint32_t** index);
  bool TranslatePackedSwitch(const Instruction* inst, const uint16_t*& pc,  int& ppc, int& dexpc,
                                  uint32_t* offset_map, uint32_t*& mem, uint32_t** index);
  bool TranslateSparseSwitch(const Instruction* inst, const uint16_t*& pc,  int& ppc, int& dexpc,
                                  uint32_t* offset_map, uint32_t*& mem, uint32_t** index);
  bool TranslateFillArrayData(const Instruction* inst, const uint16_t*& pc,  int& ppc, int& dexpc,
                                  uint32_t* offset_map, uint32_t*& mem);
  bool TranslateLiteral(const Instruction* inst, const uint16_t*& pc,  int& ppc, int& dexpc,
                                  uint32_t* offset_map, uint32_t*& mem);
  bool RelocateOne(const Instruction* inst, uint32_t* code, uint32_t reloc, int decpc);

  bool RelocateBranch(uint32_t* instptr, int offset);

  int SizeofSpecial(const Instruction* inst);

  bool BitBlat(uint32_t *code, int value, int modifier = 0);

  bool AddChunk(const Instruction* inst, const uint16_t* pc,
                      uint32_t* chunk, uint32_t* relocs, int isize, uint32_t*& mem, int dexpc);
  bool AddSharedChunk(const Instruction* inst, const uint16_t* pc,
                      uint32_t* chunk, uint32_t* relocs, int isize, uint32_t*& mem, int dexpc);
  uint32_t* PeepHole(uint32_t* program, int entrypoint_size_in_words);

  void AllocateHelperTrampoline(uint32_t* addr);
  uint32_t FindHelperTrampoline(uint32_t pc, uint32_t helper);

  // ARM instruction decoding.
  inline bool IsLoadStore(uint32_t inst) {
    int op1 = (inst >> 25) & 0x7;
    if (op1 == 2) {
      return true;
    }
    if (op1 == 3) {
      if (((inst >> 4) & 1) == 1) {
        // Media instruction, not load or store
        return false;
      }
      return true;
    }
    return false;
  }

  inline bool IsLoad(uint32_t inst) {
    return ((inst >> 20) & 1) == 1;     // Bit 20 is 1 => LDR
  }

  inline bool IsStore(uint32_t inst) {
    return ((inst >> 20) & 1) == 0;     // Bit 20 is 0 => STR
  }

  inline int GetLoadStoreRt(uint32_t inst) {
    return (inst >> 12) & 0xf;
  }

  inline int GetLoadStoreRn(uint32_t inst) {
     return (inst >> 16) & 0xf;
  }

  inline bool IsMov(uint32_t inst) {
    if (IsMovW(inst) || IsMovT(inst)) {
      return true;
    }

    // There are 2 ARM encodings A1 and A2.
    if (((inst >> 20) & 0xff) == 0b00110000) {
      // A2 encoding with a 16 bit immediate.
      return true;
    } else  if (((inst >> 21) & 0x7f) == 0b0011101) {
      // A1 encoding with 12 bit immediate.
      return true;
    }

    return false;
  }

  inline bool IsMvn(uint32_t inst) {
    return ((inst >> 21) & 0x7f) == 0b0011111;
  }

  // Must be called when we know it's an A1 or A2 move immediate.
  inline bool IsMovA2(uint32_t inst) {
    return ((inst >> 20) & 0xff) == 0b00110000;
  }

  inline bool IsMovW(uint32_t inst) {
    // Encoding T3
    if (((inst >> 27) & 0x1f) == 0b11110) {
      if (((inst >> 20)& 0x3f) == 0b100100) {
        return true;
      }
    }
    return false;
  }



  inline bool IsMovT(uint32_t inst) {
    // First check for A1 encoding.
    if (IsMovTA1(inst)) {
      return true;
    }

    // Check for encoding T1.
    if (((inst >> 27) & 0x1f) == 0b11110) {
      if (((inst >> 20)& 0x3f) == 0b101100) {
        return true;
      }
    }
    return false;
  }

  inline bool IsMovTA1(uint32_t inst) {
    if (((inst >> 20) & 0xff) == 0b00110100) {
      return true;
    }

    return false;
  }

  inline bool IsDataProcessing(uint32_t inst) {
    if (((inst >> 25) & 7) == 0b001) {
      return true;
    }
    return false;
  }


  inline bool IsShift(uint32_t inst) {
      if (((inst >> 21) & 0x7f) == 0b0001101) {
        return true;
      }
      return false;
  }

  // Rotate a 32 bit value to the left 1 bit.
  inline void rol(uint32_t& value) {
    int carry = (value & 0x80000000) != 0 ? 1 : 0;
    value = (value << 1) | carry;
  }

  inline bool IsBranch(uint32_t inst) {
    return ((inst >> 24) & 0xf) == 0b1010;
  }

  inline bool IsBranchLink(uint32_t inst) {
     return ((inst >> 24) & 0xf) == 0b1011;
  }

  inline uint16_t EncodedImmediate(uint16_t value) {
    // Find a rotated value that will fit in an ARMExpandImm(imm12) encoding
    // the encoding is an 8 bit value in the lower 8 bits, rotated right by an
    // even number of bits (the rotation held in the upper 4 bits).
    if (value < 256) {
      return value;   // Less than 9 bits is encoded immediately.
    }
    // More than 8 bits, need to find the even right rotation that gives a valid encoding.
    for (int i = 2; i < 32 ; i += 2) {
      uint32_t v = value;
      for (int j = 0; j < i; ++j) {
        rol(v);
      }
      if (v < 256) {
        // Found a rotation that works.
        return (i << 7) | v;
      }
    }
    LOG(FATAL) << "Unable to find ARM imm12 encoding for immediate value " << value;
    return 0;
  }

  // Is the address reachable by a BL instruction from the pc position?
  bool IsInBLRange(uint32_t pc, uint32_t addr) {
    int32_t offset = addr - pc;
    constexpr int32_t maxpos = 32*1024*1024;
    constexpr int32_t maxneg = -maxpos;

    return offset > maxneg && offset < maxpos;
  }
};

}   // namespace arm
}   // namespace art

#endif  // ART_RUNTIME_ARCH_ARM_XLATOR_TRANSLATOR_H_

/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_COMPILER_DEX_LOCAL_VALUE_NUMBERING_H_
#define ART_COMPILER_DEX_LOCAL_VALUE_NUMBERING_H_

#include <memory>

#include "compiler_internals.h"
#include "global_value_numbering.h"
#include "utils/scoped_arena_allocator.h"
#include "utils/scoped_arena_containers.h"

namespace art {

class DexFile;
class GlobalValueNumbering;

// Enable/disable tracking values stored in the FILLED_NEW_ARRAY result.
static constexpr bool kLocalValueNumberingEnableFilledNewArrayTracking = true;

class LocalValueNumbering {
 public:
  LocalValueNumbering(GlobalValueNumbering* gvn, ScopedArenaAllocator* allocator);

  uint16_t GetValueNumber(MIR* mir);

  // TODO: GVN
  bool Equals(const GlobalValueNumbering& other);

  // LocalValueNumbering should be allocated on the ArenaStack (or the native stack).
  static void* operator new(size_t size, ScopedArenaAllocator* allocator) {
    return allocator->Alloc(sizeof(LocalValueNumbering), kArenaAllocMIR);
  }

  // Allow delete-expression to destroy a LocalValueNumbering object without deallocation.
  static void operator delete(void* ptr) { UNUSED(ptr); }

 private:
  static constexpr uint16_t kNoValue = GlobalValueNumbering::kNoValue;

  // Field types correspond to the ordering of GET/PUT instructions; this order is the same
  // for IGET, IPUT, SGET, SPUT, AGET and APUT:
  // op         0
  // op_WIDE    1
  // op_OBJECT  2
  // op_BOOLEAN 3
  // op_BYTE    4
  // op_CHAR    5
  // op_SHORT   6
  static constexpr size_t kFieldTypeCount = 7;

  struct RangeCheckKey {
    uint16_t array;
    uint16_t index;
  };

  struct RangeCheckKeyComparator {
    bool operator()(const RangeCheckKey& lhs, const RangeCheckKey& rhs) const {
      if (lhs.array != rhs.array) {
        return lhs.array < rhs.array;
      }
      return lhs.index < rhs.index;
    }
  };

  typedef ScopedArenaSet<RangeCheckKey, RangeCheckKeyComparator> RangeCheckSet;

  typedef ScopedArenaSafeMap<uint16_t, uint16_t> AliasingIFieldVersionMap;
  typedef ScopedArenaSafeMap<uint16_t, uint16_t> NonAliasingArrayVersionMap;

  struct EscapedIFieldClobberKey {
    uint16_t base;      // Or array.
    uint16_t type;
    uint16_t field_id;  // None (kNoValue) for arrays and unresolved instance field stores.
  };

  struct EscapedIFieldClobberKeyComparator {
    bool operator()(const EscapedIFieldClobberKey& lhs, const EscapedIFieldClobberKey& rhs) const {
      if (lhs.base != rhs.base) {
        return lhs.base < rhs.base;
      }
      if (lhs.type != rhs.type) {
        return lhs.type < rhs.type;
      }
      return lhs.field_id < rhs.field_id;
    }
  };

  typedef ScopedArenaSet<EscapedIFieldClobberKey, EscapedIFieldClobberKeyComparator>
      EscapedIFieldClobberSet;

  struct EscapedArrayClobberKey {
    uint16_t base;
    uint16_t type;
  };

  struct EscapedArrayClobberKeyComparator {
    bool operator()(const EscapedArrayClobberKey& lhs, const EscapedArrayClobberKey& rhs) const {
      // Compare the type first. This allows iterating across all the entries for a certain type
      // as needed when we need to purge them for an unresolved field APUT.
      if (lhs.type != rhs.type) {
        return lhs.type < rhs.type;
      }
      // Compare the base last.
      return lhs.base < rhs.base;
    }
  };

  // Set of previously non-aliasing array refs that escaped.
  typedef ScopedArenaSet<EscapedArrayClobberKey, EscapedArrayClobberKeyComparator>
      EscapedArrayClobberSet;

  // Key is s_reg, value is value name.
  typedef ScopedArenaSafeMap<uint16_t, uint16_t> SregValueMap;
  // Key is v_reg, value is value name.
  typedef ScopedArenaSafeMap<uint16_t, uint16_t> VregValueMap;
  // Key is concatenation of opcode, operand1, operand2 and modifier, value is value name.
  typedef GlobalValueNumbering::ValueMap ValueMap;
  // Key represents a memory address, value is generation.
  // A set of value names.
  typedef ScopedArenaSet<uint16_t> ValueNameSet;

  uint16_t LookupLocalValue(uint16_t op, uint16_t operand1, uint16_t operand2, uint16_t modifier) {
    uint16_t res;
    uint64_t key = GlobalValueNumbering::BuildKey(op, operand1, operand2, modifier);
    ValueMap::iterator it = local_value_map_.find(key);
    if (it != local_value_map_.end()) {
      res = it->second;
    } else {
      res = gvn_->NewValueName();
      local_value_map_.Put(key, res);
    }
    return res;
  };

  void StoreLocalValue(uint16_t op, uint16_t operand1, uint16_t operand2, uint16_t modifier,
                       uint16_t value) {
    uint64_t key = GlobalValueNumbering::BuildKey(op, operand1, operand2, modifier);
    local_value_map_.Overwrite(key, value);
  }

  // Find local value if exists, otherwise get global value.
  uint16_t LookupValuePreferLocal(uint16_t op, uint16_t operand1, uint16_t operand2,
                                  uint16_t modifier) {
    uint64_t key = GlobalValueNumbering::BuildKey(op, operand1, operand2, modifier);
    ValueMap::iterator it = local_value_map_.find(key);
    if (it != local_value_map_.end()) {
      return it->second;
    } else {
      return gvn_->LookupValue(op, operand1, operand2, modifier);
    }
  }

  bool HasValuePreferLocal(uint16_t op, uint16_t operand1, uint16_t operand2, uint16_t modifier,
                           uint16_t value) const {
    uint64_t key = GlobalValueNumbering::BuildKey(op, operand1, operand2, modifier);
    ValueMap::const_iterator it = local_value_map_.find(key);
    if (it != local_value_map_.end()) {
      return it->second == value;
    } else {
      return gvn_->HasValue(op, operand1, operand2, modifier, value);
    }
  };

  void SetOperandValue(uint16_t s_reg, uint16_t value) {
    // FIXME: Remove the DCHECK when we handle GVN for BBs with multiple predecessors.
    // We're using Overwrite() for performance (only one lookup in release mode)
    // but if the value exists, it must not change.
    DCHECK(sreg_value_map_.find(s_reg) == sreg_value_map_.end() ||
           sreg_value_map_.Get(s_reg) == value);
    sreg_value_map_.Overwrite(s_reg, value);
  };

  uint16_t GetOperandValue(int s_reg) {
    uint16_t res = kNoValue;
    SregValueMap::iterator it = sreg_value_map_.find(s_reg);
    if (it != sreg_value_map_.end()) {
      res = it->second;
    } else {
      // First use
      res = gvn_->LookupValue(kNoValue, s_reg, kNoValue, kNoValue);
      sreg_value_map_.Put(s_reg, res);
    }
    return res;
  };

  void SetOperandValueWide(uint16_t s_reg, uint16_t value) {
    // FIXME: Remove the DCHECK when we handle GVN for BBs with multiple predecessors.
    // We're using Overwrite() for performance (only one lookup in release mode)
    // but if the value exists, it must not change.
    DCHECK(sreg_wide_value_map_.find(s_reg) == sreg_wide_value_map_.end() ||
           sreg_wide_value_map_.Get(s_reg) == value);
    sreg_wide_value_map_.Overwrite(s_reg, value);
  };

  uint16_t GetOperandValueWide(int s_reg) {
    uint16_t res = kNoValue;
    SregValueMap::iterator it = sreg_wide_value_map_.find(s_reg);
    if (it != sreg_wide_value_map_.end()) {
      res = it->second;
    } else {
      // First use
      res = gvn_->LookupValue(kNoValue, s_reg, kNoValue, kNoValue);
      sreg_wide_value_map_.Put(s_reg, res);
    }
    return res;
  };

  uint16_t MarkNonAliasingNonNull(MIR* mir);
  bool IsNonAliasing(uint16_t reg);
  bool IsNonAliasingIField(uint16_t reg, uint16_t field_id, uint16_t type);
  bool IsNonAliasingArray(uint16_t reg, uint16_t type);
  void HandleNullCheck(MIR* mir, uint16_t reg);
  void HandleRangeCheck(MIR* mir, uint16_t array, uint16_t index);
  void HandlePutObject(MIR* mir);
  void HandleEscapingRef(uint16_t base);
  uint16_t HandleAGet(MIR* mir, uint16_t opcode);
  void HandleAPut(MIR* mir, uint16_t opcode);
  uint16_t HandleIGet(MIR* mir, uint16_t opcode);
  void HandleIPut(MIR* mir, uint16_t opcode);
  uint16_t HandleSGet(MIR* mir, uint16_t opcode);
  void HandleSPut(MIR* mir, uint16_t opcode);

  GlobalValueNumbering* gvn_;

  SregValueMap sreg_value_map_;
  SregValueMap sreg_wide_value_map_;
  ValueMap local_value_map_;

  // Data for dealing with memory clobbering and store/load aliasing.
  uint16_t global_memory_version_;
  uint16_t unresolved_sfield_version_[kFieldTypeCount];
  uint16_t unresolved_ifield_version_[kFieldTypeCount];
  uint16_t aliasing_array_version_[kFieldTypeCount];
  AliasingIFieldVersionMap aliasing_ifield_version_map_;
  NonAliasingArrayVersionMap non_aliasing_array_version_map_;
  // Value names of references to objects that cannot be reached through a different value name.
  ValueNameSet non_aliasing_refs_;
  // Previously non-aliasing refs that escaped but can still be used for non-aliasing AGET/IGET.
  ValueNameSet escaped_refs_;
  // Blacklists for cases where escaped_refs_ can't be used.
  EscapedIFieldClobberSet escaped_ifield_clobber_set_;
  EscapedArrayClobberSet escaped_array_clobber_set_;

  // Range check and null check elimination.
  RangeCheckSet range_checked_;
  ValueNameSet null_checked_;

  DISALLOW_COPY_AND_ASSIGN(LocalValueNumbering);
};

}  // namespace art

#endif  // ART_COMPILER_DEX_LOCAL_VALUE_NUMBERING_H_

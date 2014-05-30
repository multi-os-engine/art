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

// Enable/disable tracking values stored in the FILLED_NEW_ARRAY result.
static constexpr bool kLocalValueNumberingEnableFilledNewArrayTracking = true;

class LocalValueNumbering {
 public:
  LocalValueNumbering(GlobalValueNumbering* gvn, ScopedArenaAllocator* allocator, uint16_t id);

  bool Equals(const LocalValueNumbering& other) const;

  // Set non-static method's "this".
  void SetNullChecked(int sreg) {
    uint16_t res = GetOperandValue(sreg);
    null_checked_.insert(res);
  }

  bool IsNullChecked(int sreg) {
    uint16_t res = GetOperandValue(sreg);
    return null_checked_.find(res) != null_checked_.end();
  }

  void Copy(const LocalValueNumbering& other);
  void Merge(const LocalValueNumbering& other);

  uint16_t GetValueNumber(MIR* mir);

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

    // NOTE: Can't define this at namespace scope for a private struct.
    bool operator==(const RangeCheckKey& other) const {
      return array == other.array && index == other.index;
    }
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

  // Maps instance field "location" (derived from base, field_id and type) to value name.
  typedef ScopedArenaSafeMap<uint16_t, uint16_t> IFieldLocToValueMap;

  // Maps static field id to value name
  typedef ScopedArenaSafeMap<uint16_t, uint16_t> SFieldToValueMap;

  // TODO: Describe.
  typedef ScopedArenaSafeMap<uint16_t, uint16_t> AliasingIFieldVersionMap;
  // TODO: Describe.
  typedef ScopedArenaSafeMap<uint16_t, uint16_t> NonAliasingArrayVersionMap;

  struct EscapedIFieldClobberKey {
    uint16_t base;      // Or array.
    uint16_t type;
    uint16_t field_id;  // None (kNoValue) for arrays and unresolved instance field stores.

    // NOTE: Can't define this at namespace scope for a private struct.
    bool operator==(const EscapedIFieldClobberKey& other) const {
      return base == other.base && type == other.type && field_id == other.field_id;
    }
  };

  struct EscapedIFieldClobberKeyComparator {
    bool operator()(const EscapedIFieldClobberKey& lhs, const EscapedIFieldClobberKey& rhs) const {
      // Compare base first. This makes sequential iteration respect the order of base.
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

    // NOTE: Can't define this at namespace scope for a private struct.
    bool operator==(const EscapedArrayClobberKey& other) const {
      return base == other.base && type == other.type;
    }
  };

  struct EscapedArrayClobberKeyComparator {
    bool operator()(const EscapedArrayClobberKey& lhs, const EscapedArrayClobberKey& rhs) const {
      // Compare base first. This makes sequential iteration respect the order of base.
      if (lhs.base != rhs.base) {
        return lhs.base < rhs.base;
      }
      return lhs.type < rhs.type;
    }
  };

  // Set of previously non-aliasing array refs that escaped.
  typedef ScopedArenaSet<EscapedArrayClobberKey, EscapedArrayClobberKeyComparator>
      EscapedArrayClobberSet;

  // Key is s_reg, value is value name.
  typedef ScopedArenaSafeMap<uint16_t, uint16_t> SregValueMap;
  // A set of value names.
  typedef ScopedArenaSet<uint16_t> ValueNameSet;

  void SetOperandValue(uint16_t s_reg, uint16_t value) {
    sreg_value_map_.PutIfMissing(s_reg, value);
  };

  uint16_t GetOperandValue(int s_reg) {
    uint16_t res = kNoValue;
    auto lb = sreg_value_map_.lower_bound(s_reg);
    if (lb != sreg_value_map_.end() && lb->first == s_reg) {
      res = lb->second;
    } else {
      // First use
      res = gvn_->LookupValue(kNoValue, s_reg, kNoValue, kNoValue);
      sreg_value_map_.Put(lb, s_reg, res);
    }
    return res;
  };

  void SetOperandValueWide(uint16_t s_reg, uint16_t value) {
    sreg_wide_value_map_.PutIfMissing(s_reg, value);
  };

  uint16_t GetOperandValueWide(int s_reg) {
    uint16_t res = kNoValue;
    auto lb = sreg_wide_value_map_.lower_bound(s_reg);
    if (lb != sreg_wide_value_map_.end() && lb->first == s_reg) {
      res = lb->second;
    } else {
      // First use
      res = gvn_->LookupValue(kNoValue, s_reg, kNoValue, kNoValue);
      sreg_wide_value_map_.Put(lb, s_reg, res);
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
  void RemoveSFieldsForType(uint16_t type);

  bool SameMemoryVersion(const LocalValueNumbering& other) const;

  static void MergeSregMap(SregValueMap* work_map, const SregValueMap& other_map);

  template <typename K, typename V, typename Comparator>
  void MergeLocalMap(ScopedArenaSafeMap<K, V, Comparator>* work_map,
                     const ScopedArenaSafeMap<K, V, Comparator>& other_map,
                     uint16_t bump_op);

  // Merge escaped_refs_. Must be called before calculating the new non-aliasing refs.
  void MergeEscapedRefs(const LocalValueNumbering& other);

  // Merge a clobber set. Must be called after calculating the new escaped refs.
  template <typename K, typename Comparator>
  void MergeClobberSet(ScopedArenaSet<K, Comparator>* work_clobber_set,
                       const ScopedArenaSet<K, Comparator>& other_clobber_set);

  template <typename K, typename Comparator>
  static void Intersect(ScopedArenaSet<K, Comparator>* work_set,
                        const ScopedArenaSet<K, Comparator>& other_set);

  GlobalValueNumbering* gvn_;
  uint16_t id_;

  SregValueMap sreg_value_map_;
  SregValueMap sreg_wide_value_map_;
  IFieldLocToValueMap non_aliasing_ifield_value_map_;
  SFieldToValueMap sfield_value_map_;

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

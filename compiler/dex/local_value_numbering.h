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
  LocalValueNumbering(GlobalValueNumbering* gvn, uint16_t id);

  uint16_t Id() const {
    return id_;
  }

  void SetCatchEntry();

  bool Equals(const LocalValueNumbering& other) const;

  // Set non-static method's "this".
  void SetValueNullChecked(int value_name) {
    null_checked_.insert(value_name);
  }

  bool IsValueNullChecked(int value_name) const {
    return null_checked_.find(value_name) != null_checked_.end();
  }

  void Copy(const LocalValueNumbering& other);
  void Merge();  // Merge gvn_->merge_lvns_.
  void Merge(const LocalValueNumbering& other);
  void Finish();

  uint16_t GetValueNumber(MIR* mir);

  // LocalValueNumbering should be allocated on the ArenaStack (or the native stack).
  static void* operator new(size_t size, ScopedArenaAllocator* allocator) {
    return allocator->Alloc(sizeof(LocalValueNumbering), kArenaAllocMIR);
  }

  // Allow delete-expression to destroy a LocalValueNumbering object without deallocation.
  static void operator delete(void* ptr) { UNUSED(ptr); }

 private:
  static constexpr uint16_t kNoValue = GlobalValueNumbering::kNoValue;

  // A set of value names.
  typedef GlobalValueNumbering::ValueNameSet ValueNameSet;

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
      // Compare type second. This makes the type-clobber entries (field_id == kNoValue) last
      // for given base and type and makes it easy to prune unnecessary entries when merging
      // escaped_ifield_clobber_set_ from multiple LVNs.
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

  // Clobber set for previously non-aliasing array refs that escaped.
  typedef ScopedArenaSet<EscapedArrayClobberKey, EscapedArrayClobberKeyComparator>
      EscapedArrayClobberSet;

  // Known values for an instance field.
  struct AliasingIFieldValues {
    explicit AliasingIFieldValues(ScopedArenaAllocator* allocator)
        : memory_version_before_stores(kNoValue),
          last_stored_value(kNoValue),
          store_ref_set(std::less<uint16_t>(), allocator->Adapter()),
          last_load_memory_version(kNoValue),
          load_value_map(std::less<uint16_t>(), allocator->Adapter()) {
    }

    uint16_t memory_version_before_stores;  // kNoValue if start version for the field.
    uint16_t last_stored_value;             // Last stored value name, kNoValue if none.
    ValueNameSet store_ref_set;             // Where was last_stored_value stored.

    // Maps refs (other than stored_to) to currently known values for this field other. On write,
    // anything that differs from the written value is removed as it may be overwritten.
    uint16_t last_load_memory_version;    // kNoValue if not known.
    ScopedArenaSafeMap<uint16_t, uint16_t> load_value_map;

    // NOTE: Can't define this at namespace scope for a private struct.
    bool operator==(const AliasingIFieldValues& other) const {
      return memory_version_before_stores == other.memory_version_before_stores &&
          last_load_memory_version == other.last_load_memory_version &&
          last_stored_value == other.last_stored_value &&
          store_ref_set == other.store_ref_set &&
          load_value_map == other.load_value_map;
    }
  };

  // Maps instance field id to AliasingIFieldValues.
  typedef ScopedArenaSafeMap<uint16_t, AliasingIFieldValues> AliasingIFieldValuesMap;

  uint16_t MarkNonAliasingNonNull(MIR* mir);
  bool IsNonAliasing(uint16_t reg);
  bool IsNonAliasingIField(uint16_t reg, uint16_t field_id, uint16_t type);
  bool IsNonAliasingArray(uint16_t reg, uint16_t type);
  void HandleNullCheck(MIR* mir, uint16_t reg);
  void HandleRangeCheck(MIR* mir, uint16_t array, uint16_t index);
  void HandlePutObject(MIR* mir);
  void HandleEscapingRef(uint16_t base);
  uint16_t HandlePhi(MIR* mir);
  uint16_t HandleAGet(MIR* mir, uint16_t opcode);
  void HandleAPut(MIR* mir, uint16_t opcode);
  AliasingIFieldValues* GetAliasingIFieldValues(uint16_t field_id);
  void UpdateAliasingIFieldMemoryVersion(uint16_t field_id, uint16_t type,
                                         AliasingIFieldValues* values);
  uint16_t HandleIGet(MIR* mir, uint16_t opcode);
  void HandleIPut(MIR* mir, uint16_t opcode);
  uint16_t HandleSGet(MIR* mir, uint16_t opcode);
  void HandleSPut(MIR* mir, uint16_t opcode);
  void RemoveSFieldsForType(uint16_t type);

  bool SameMemoryVersion(const LocalValueNumbering& other) const;

  uint16_t NewMemoryVersion(uint16_t* new_version);
  void MergeMemoryVersions();

  template <typename Set, Set LocalValueNumbering::* set_ptr>
  void IntersectSets();

  template <typename Set, Set LocalValueNumbering::*set_ptr, void (LocalValueNumbering::*MergeFn)(
      const typename Set::value_type& entry, typename Set::iterator hint)>
  void MergeSets();

  template <typename Map>
  static void InPlaceMapUnion(Map* work_map, const Map& other_map);

  void MergeAliasingIFieldValueRefs(AliasingIFieldValues* work_values,
                                    const AliasingIFieldValues* values);

  void MergeEscapedRefs(const ValueNameSet::value_type& entry, ValueNameSet::iterator hint);
  void MergeEscapedIFieldTypeClobberSets(const EscapedIFieldClobberSet::value_type& entry,
                                         EscapedIFieldClobberSet::iterator hint);
  void MergeEscapedIFieldClobberSets(const EscapedIFieldClobberSet::value_type& entry,
                                     EscapedIFieldClobberSet::iterator hint);
  void MergeEscapedArrayClobberSets(const EscapedArrayClobberSet::value_type& entry,
                                    EscapedArrayClobberSet::iterator hint);
  void MergeNullChecked(const ValueNameSet::value_type& entry, ValueNameSet::iterator hint);
  void MergeSFieldValues(const SFieldToValueMap::value_type& entry,
                         SFieldToValueMap::iterator hint);
  void MergeNonAliasingIFieldValues(const IFieldLocToValueMap::value_type& entry,
                                    IFieldLocToValueMap::iterator hint);
  void MergeAliasingIFieldValues(const AliasingIFieldValuesMap::value_type& entry,
                                 AliasingIFieldValuesMap::iterator hint);

  template <typename K, typename V, typename Comparator>
  void MergeLocalMap(ScopedArenaSafeMap<K, V, Comparator>* work_map,
                     const ScopedArenaSafeMap<K, V, Comparator>& other_map,
                     uint16_t bump_op);

  GlobalValueNumbering* gvn_;
  uint16_t id_;

  SFieldToValueMap sfield_value_map_;
  IFieldLocToValueMap non_aliasing_ifield_value_map_;
  AliasingIFieldValuesMap aliasing_ifield_value_map_;

  // Data for dealing with memory clobbering and store/load aliasing.
  uint16_t global_memory_version_;
  uint16_t unresolved_sfield_version_[kFieldTypeCount];
  uint16_t unresolved_ifield_version_[kFieldTypeCount];
  uint16_t aliasing_array_version_[kFieldTypeCount];
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

  // Reuse one vector for all merges to avoid leaking too much memory on the ArenaStack.
  ScopedArenaVector<BasicBlockId> merge_names_;
  // Map to identify when different locations merge the same values.
  ScopedArenaSafeMap<ScopedArenaVector<BasicBlockId>, uint16_t> merge_map_;
  // New memory version for merge, kNoValue if all memory versions matched.
  uint16_t merge_new_memory_version_;

  DISALLOW_COPY_AND_ASSIGN(LocalValueNumbering);
};

}  // namespace art

#endif  // ART_COMPILER_DEX_LOCAL_VALUE_NUMBERING_H_

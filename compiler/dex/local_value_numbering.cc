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

#include "local_value_numbering.h"

#include "global_value_numbering.h"
#include "mir_field_info.h"
#include "mir_graph.h"

namespace art {

namespace {  // anonymous namespace

// Operations used for value map keys instead of actual opcode.
static constexpr uint16_t kInvokeMemoryVersionBumpOp = Instruction::INVOKE_VIRTUAL;
static constexpr uint16_t kUnresolvedSFieldOp = Instruction::SGET;
static constexpr uint16_t kResolvedSFieldOp = Instruction::SGET_WIDE;
static constexpr uint16_t kUnresolvedIFieldOp = Instruction::IGET;
static constexpr uint16_t kNonAliasingIFieldLocOp = Instruction::IGET_WIDE;
static constexpr uint16_t kNonAliasingIFieldInitialOp = Instruction::IGET_OBJECT;
static constexpr uint16_t kAliasingIFieldOp = Instruction::IGET_BOOLEAN;
static constexpr uint16_t kAliasingIFieldStartVersionOp = Instruction::IGET_BYTE;
static constexpr uint16_t kAliasingIFieldBumpVersionOp = Instruction::IGET_CHAR;
static constexpr uint16_t kArrayAccessLocOp = Instruction::AGET;
static constexpr uint16_t kNonAliasingArrayOp = Instruction::AGET_WIDE;
static constexpr uint16_t kNonAliasingArrayStartVersionOp = Instruction::AGET_OBJECT;
static constexpr uint16_t kAliasingArrayOp = Instruction::AGET_BOOLEAN;
static constexpr uint16_t kAliasingArrayMemoryVersionOp = Instruction::AGET_BYTE;
static constexpr uint16_t kAliasingArrayBumpVersionOp = Instruction::AGET_CHAR;
static constexpr uint16_t kMergeBlockMemoryVersionBumpOp = Instruction::INVOKE_VIRTUAL_RANGE;
static constexpr uint16_t kMergeBlockAliasingIFieldVersionBumpOp = Instruction::IPUT;
static constexpr uint16_t kMergeBlockNonAliasingArrayVersionBumpOp = Instruction::APUT;
static constexpr uint16_t kMergeBlockNonAliasingIFieldVersionBumpOp = Instruction::APUT_WIDE;
static constexpr uint16_t kMergeBlockSFieldVersionBumpOp = Instruction::APUT_OBJECT;

}  // anonymous namespace

LocalValueNumbering::LocalValueNumbering(GlobalValueNumbering* gvn, uint16_t id)
    : gvn_(gvn),
      id_(id),
      non_aliasing_ifield_value_map_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      sfield_value_map_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      global_memory_version_(0u),
      aliasing_ifield_version_map_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      non_aliasing_array_version_map_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      non_aliasing_refs_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      escaped_refs_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      escaped_ifield_clobber_set_(EscapedIFieldClobberKeyComparator(), gvn->Allocator()->Adapter()),
      escaped_array_clobber_set_(EscapedArrayClobberKeyComparator(), gvn->Allocator()->Adapter()),
      range_checked_(RangeCheckKeyComparator() , gvn->Allocator()->Adapter()),
      null_checked_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      merge_names_(gvn->Allocator()->Adapter()),
      merge_map_(std::less<ScopedArenaVector<BasicBlockId>>(), gvn->Allocator()->Adapter()) {
  std::fill_n(unresolved_sfield_version_, kFieldTypeCount, 0u);
  std::fill_n(unresolved_ifield_version_, kFieldTypeCount, 0u);
  std::fill_n(aliasing_array_version_, kFieldTypeCount, 0u);
}

void LocalValueNumbering::SetCatchEntry() {
  // Use kMergeBlockMemoryVersionBumpOp, it's unique for each BB.
  global_memory_version_ = gvn_->LookupValue(kMergeBlockMemoryVersionBumpOp, 0u, 0u, id_);
}

bool LocalValueNumbering::Equals(const LocalValueNumbering& other) const {
  DCHECK(gvn_ == other.gvn_);
  // Compare the maps/sets and memory versions.
  return non_aliasing_ifield_value_map_ == other.non_aliasing_ifield_value_map_ &&
      sfield_value_map_ == other.sfield_value_map_ &&
      SameMemoryVersion(other) &&
      aliasing_ifield_version_map_ == other.aliasing_ifield_version_map_ &&
      non_aliasing_array_version_map_ == other.non_aliasing_array_version_map_ &&
      non_aliasing_refs_ == other.non_aliasing_refs_ &&
      escaped_refs_ == other.escaped_refs_ &&
      escaped_ifield_clobber_set_ == other.escaped_ifield_clobber_set_ &&
      escaped_array_clobber_set_ == other.escaped_array_clobber_set_ &&
      range_checked_ == other.range_checked_ &&
      null_checked_ == other.null_checked_;
}

bool LocalValueNumbering::SameMemoryVersion(const LocalValueNumbering& other) const {
  return
      global_memory_version_ == other.global_memory_version_ &&
      std::equal(unresolved_ifield_version_, unresolved_ifield_version_ + kFieldTypeCount,
                 other.unresolved_ifield_version_) &&
      std::equal(unresolved_sfield_version_, unresolved_sfield_version_ + kFieldTypeCount,
                 other.unresolved_sfield_version_) &&
      std::equal(aliasing_array_version_, aliasing_array_version_ + kFieldTypeCount,
                 other.aliasing_array_version_);
}

void LocalValueNumbering::Copy(const LocalValueNumbering& other) {
  non_aliasing_ifield_value_map_ = other.non_aliasing_ifield_value_map_;
  sfield_value_map_ = other.sfield_value_map_;
  global_memory_version_ = other.global_memory_version_;
  aliasing_ifield_version_map_ = other.aliasing_ifield_version_map_;
  non_aliasing_array_version_map_ = other.non_aliasing_array_version_map_;
  non_aliasing_refs_ = other.non_aliasing_refs_;
  escaped_refs_ = other.escaped_refs_;
  escaped_ifield_clobber_set_ = other.escaped_ifield_clobber_set_;
  escaped_array_clobber_set_ = other.escaped_array_clobber_set_;
  range_checked_ = other.range_checked_;
  null_checked_ = other.null_checked_;
  std::copy(other.unresolved_ifield_version_, other.unresolved_ifield_version_ + kFieldTypeCount,
            unresolved_ifield_version_);
  std::copy(other.unresolved_sfield_version_, other.unresolved_sfield_version_ + kFieldTypeCount,
            unresolved_sfield_version_);
  std::copy(other.aliasing_array_version_, other.aliasing_array_version_ + kFieldTypeCount,
            aliasing_array_version_);
}

template <typename K, typename V, typename Comparator>
void LocalValueNumbering::MergeLocalMap(ScopedArenaSafeMap<K, V, Comparator>* work_map,
                                        const ScopedArenaSafeMap<K, V, Comparator>& other_map,
                                        uint16_t bump_op) {
  // TODO: Keep a map of merge sets (a set of all incoming id, value name pairs) in GVN
  // so that we can asign the same value if multiple locations merge the same value names.
  // Then merge all the bump_ops into one.
  Comparator cmp;  // All our comparators are stateless and default-constructible.
  auto it = work_map->begin(), end = work_map->end();
  auto other_it = other_map.begin(), other_end = other_map.end();
  while (it != end || other_it != other_end) {
    if (it == end || (other_it != end && cmp(other_it->first, it->first))) {
      uint16_t value = gvn_->LookupValue(bump_op, it->first, id_, kNoValue);
      work_map->PutHint(it, other_it->first, value);
      ++other_it;
    } else if (other_it == other_end || (it != end && cmp(it->first, other_it->first))) {
      uint16_t value = gvn_->LookupValue(bump_op, it->first, id_, kNoValue);
      it->second = value;
      ++it;
    } else {
      if (it->second != other_it->second) {
        uint16_t value = gvn_->LookupValue(bump_op, it->first, id_, kNoValue);
        it->second = value;
      }
      ++it;
      ++other_it;
    }
  }
}

template <typename K, typename Comparator>
void LocalValueNumbering::MergeClobberSet(ScopedArenaSet<K, Comparator>* work_clobber_set,
                                          const ScopedArenaSet<K, Comparator>& other_clobber_set) {
  auto cs_it = work_clobber_set->begin(), cs_end = work_clobber_set->end();
  auto other_cs_it = other_clobber_set.begin(), other_cs_end = other_clobber_set.end();
  for (uint16_t base : escaped_refs_) {  // Iteration in increasing order.
    while (cs_it != cs_end && cs_it->base < base) {
      cs_it = work_clobber_set->erase(cs_it);  // Remove obsolete clobber entry.
    }
    while (other_cs_it != other_cs_end && other_cs_it->base < base) {
      ++other_cs_it;  // Skip obsolete clobber entry in other_clobber_set.
    }
    // Now merge the entries for base.
    while ((cs_it != cs_end && cs_it->base == base) ||
        (other_cs_it != other_cs_end && other_cs_it->base == base)) {
      Comparator cmp;  // All our comparators are stateless and default-constructible.
      if (cs_it == cs_end || (other_cs_it != other_cs_end && cmp(*other_cs_it, *cs_it))) {
        work_clobber_set->insert(cs_it, *other_cs_it);
        ++other_cs_it;
      } else if (other_cs_it == other_cs_end || (cs_it != cs_end && cmp(*cs_it, *other_cs_it))) {
        // *cs_it is already in this->escaped_ifield_clobber_set_.
        ++cs_it;
      } else {
        ++cs_it;
        ++other_cs_it;
      }
    }
  }
  // Remove clobber entries for refs after the last base.
  work_clobber_set->erase(cs_it, cs_end);
}

template <typename K, typename Comparator>
void LocalValueNumbering::Intersect(ScopedArenaSet<K, Comparator>* work_set,
                                    const ScopedArenaSet<K, Comparator>& other_set) {
  // In-place intersection; *work_set = Intersect(*work_set, other).
  Comparator cmp = work_set->key_comp();
  auto it = work_set->begin(), end = work_set->end();
  auto other_it = other_set.begin(), other_end = other_set.end();
  while (it != end) {
    while (other_it != other_end && cmp(*other_it, *it)) {
      ++other_it;
    }
    if (other_it != other_end && !cmp(*it, *other_it)) {
      ++it;
      ++other_it;
    } else {
      it = work_set->erase(it);
    }
  }
}

uint16_t LocalValueNumbering::MergeSFieldValues(
    const ScopedArenaVector<const LocalValueNumbering*>& lvns, uint16_t field_id) {
  merge_names_.reserve(lvns.size());
  merge_names_.clear();
  uint16_t value_name = kNoValue;
  bool same_values = true;
  for (const LocalValueNumbering* lvn : lvns) {
    // Get the value name as in HandleSGet() but don't modify *lvn.
    auto it = lvn->sfield_value_map_.find(field_id);
    if (it != lvn->sfield_value_map_.end()) {
      value_name = it->second;
    } else {
      uint16_t type = gvn_->GetFieldType(field_id);
      value_name = gvn_->LookupValue(kResolvedSFieldOp, field_id,
                                     lvn->unresolved_sfield_version_[type],
                                     lvn->global_memory_version_);
    }

    same_values = same_values && (merge_names_.empty() || value_name == merge_names_.back());
    merge_names_.push_back(value_name);
  }
  if (same_values) {
    // value_name already contains the result.
  } else {
    auto lb = merge_map_.lower_bound(merge_names_);
    if (lb != merge_map_.end() && !merge_map_.key_comp()(merge_names_, lb->first)) {
      value_name = lb->second;
    } else {
      value_name = gvn_->LookupValue(kMergeBlockSFieldVersionBumpOp, field_id, id_, kNoValue);
      merge_map_.PutHint(lb, merge_names_, value_name);
      if (gvn_->NullCheckedInAllPredecessors(*this, lvns, merge_names_)) {
        null_checked_.insert(value_name);
      }
    }
  }
  return value_name;
}

uint16_t LocalValueNumbering::MergeNonAliasingIFieldValues(
    const ScopedArenaVector<const LocalValueNumbering*>& lvns, uint16_t field_loc) {
  merge_names_.reserve(lvns.size());
  merge_names_.clear();
  uint16_t value_name = kNoValue;
  bool same_values = true;
  for (const LocalValueNumbering* lvn : lvns) {
    // Get the value name as in HandleIGet() but don't modify *lvn.
    auto it = lvn->non_aliasing_ifield_value_map_.find(field_loc);
    if (it != lvn->non_aliasing_ifield_value_map_.end()) {
      value_name = it->second;
    } else {
      value_name = gvn_->LookupValue(kNonAliasingIFieldInitialOp, field_loc, kNoValue, kNoValue);
    }

    same_values = same_values && (merge_names_.empty() || value_name == merge_names_.back());
    merge_names_.push_back(value_name);
  }
  if (same_values) {
    // value_name already contains the result.
  } else {
    auto lb = merge_map_.lower_bound(merge_names_);
    if (lb != merge_map_.end() && !merge_map_.key_comp()(merge_names_, lb->first)) {
      value_name = lb->second;
    } else {
      value_name = gvn_->LookupValue(kMergeBlockNonAliasingIFieldVersionBumpOp, field_loc,
                                     id_, kNoValue);
      merge_map_.PutHint(lb, merge_names_, value_name);
      if (gvn_->NullCheckedInAllPredecessors(*this, lvns, merge_names_)) {
        null_checked_.insert(value_name);
      }
    }
  }
  return value_name;
}

void LocalValueNumbering::MergeSFieldValues(
    const ScopedArenaVector<const LocalValueNumbering*>& lvns) {
  for (const LocalValueNumbering* lvn : lvns) {
    auto my_it = sfield_value_map_.begin(), my_end = sfield_value_map_.end();
    for (const auto& entry : lvn->sfield_value_map_) {
      while (my_it != my_end && my_it->first < entry.first) {
        ++my_it;
      }
      if (my_it != my_end && my_it->first == entry.first) {
        // Already handled.
        ++my_it;
      } else {
        // Merge values for this field_id.
        uint16_t value_name = MergeSFieldValues(lvns, entry.first);
        sfield_value_map_.PutHint(my_it, entry.first, value_name);  // my_it remains valid.
      }
    }
  }
}

void LocalValueNumbering::MergeNonAliasingIFieldValues(
    const ScopedArenaVector<const LocalValueNumbering*>& lvns) {
  for (const LocalValueNumbering* lvn : lvns) {
    auto my_it = non_aliasing_ifield_value_map_.begin(),
        my_end = non_aliasing_ifield_value_map_.end();
    for (const auto& entry : lvn->non_aliasing_ifield_value_map_) {
      while (my_it != my_end && my_it->first < entry.first) {
        ++my_it;
      }
      if (my_it != my_end && my_it->first == entry.first) {
        // Already handled.
        ++my_it;
      } else {
        // Merge values for this field_loc.
        uint16_t value_name = MergeNonAliasingIFieldValues(lvns, entry.first);
        non_aliasing_ifield_value_map_.PutHint(my_it, entry.first, value_name);  // my_it remains valid.
      }
    }
  }
}

void LocalValueNumbering::MergeEscapedRefs(
    const ScopedArenaVector<const LocalValueNumbering*>& lvns) {
  for (const LocalValueNumbering* lvn : lvns) {
    auto my_it = escaped_refs_.begin(), my_end = escaped_refs_.end();
    for (const auto& entry : lvn->escaped_refs_) {
      while (my_it != my_end && escaped_refs_.key_comp()(*my_it, entry)) {
        ++my_it;
      }
      if (my_it != my_end && !escaped_refs_.key_comp()(entry, *my_it)) {
        // Already handled.
        ++my_it;
      } else {
        // See if the ref is either escaped or non-aliasing in each predecessor.
        bool is_escaped = true;
        for (const LocalValueNumbering* inner_lvn : lvns) {
          if (inner_lvn->non_aliasing_refs_.count(entry) == 0u &&
              inner_lvn->escaped_refs_.count(entry) == 0u) {
            is_escaped = false;
            break;
          }
        }
        if (is_escaped) {
          escaped_refs_.emplace_hint(my_it, entry);  // my_it remains valid.
        }
      }
    }
  }
}

void LocalValueNumbering::MergeEscapedIFieldClobberSets(
    const ScopedArenaVector<const LocalValueNumbering*>& lvns) {
  for (const LocalValueNumbering* lvn : lvns) {
    auto my_it = escaped_ifield_clobber_set_.begin(), my_end = escaped_ifield_clobber_set_.end();
    for (const auto& entry : lvn->escaped_ifield_clobber_set_) {
      while (my_it != my_end && escaped_ifield_clobber_set_.key_comp()(*my_it, entry)) {
        ++my_it;
      }
      if (my_it != my_end && !escaped_ifield_clobber_set_.key_comp()(entry, *my_it)) {
        // Already handled.
        ++my_it;
      } else {
        // See if the ref is still escaped.
        if (escaped_refs_.count(entry.base)) {
          // TODO: Prune field id clobbers for clobbered types.
          escaped_ifield_clobber_set_.emplace_hint(my_it, entry);
        }
      }
    }
  }
}

template <typename Set, Set LocalValueNumbering::* set_ptr>
void LocalValueNumbering::IntersectSets(
    const ScopedArenaVector<const LocalValueNumbering*>& lvns) {
  DCHECK_GE(lvns.size(), 2u);

  // Find the LVN with the least entries in the set.
  const LocalValueNumbering* least_entries_lvn = lvns[0];
  for (const LocalValueNumbering* lvn : lvns) {
    if ((lvn->*set_ptr).size() < (least_entries_lvn->*set_ptr).size()) {
      least_entries_lvn = lvn;
    }
  }

  // For each key check if it's in all the LVNs.
  for (const auto& key : least_entries_lvn->*set_ptr) {
    bool checked = true;
    for (const LocalValueNumbering* lvn : lvns) {
      if (lvn != least_entries_lvn && (lvn->*set_ptr).count(key) == 0u) {
        checked = false;
        break;
      }
    }
    if (checked) {
      (this->*set_ptr).insert(key);
    }
  }
}

void LocalValueNumbering::MergeNullChecked(
    const ScopedArenaVector<const LocalValueNumbering*>& lvns) {
  DCHECK_GE(lvns.size(), 2u);
  for (const LocalValueNumbering* lvn : lvns) {
    auto my_it = null_checked_.begin(), my_end = null_checked_.end();
    for (const auto& entry : lvn->null_checked_) {
      while (my_it != my_end && *my_it < entry) {
        ++my_it;
      }
      if (*my_it == entry) {
        // Already handled.
        ++my_it;
      } else {
        // Merge null_checked_ for this ref.
        merge_names_.clear();
        merge_names_.resize(lvns.size(), entry);
        if (gvn_->NullCheckedInAllPredecessors(*this, lvns, merge_names_)) {
          null_checked_.insert(my_it, entry);  // my_it remains valid.
        }
      }
    }
  }
}

void LocalValueNumbering::Merge(const ScopedArenaVector<const LocalValueNumbering*>& lvns) {
  DCHECK_GE(lvns.size(), 2u);

  // We won't do anything complicated for range checks, just calculate the intersection.
  IntersectSets<RangeCheckSet, &LocalValueNumbering::range_checked_>(lvns);

  // Intersect the non-aliasing refs and merge escaped refs and clobber sets.
  IntersectSets<ValueNameSet, &LocalValueNumbering::non_aliasing_refs_>(lvns);
  MergeEscapedRefs(lvns);
  MergeEscapedIFieldClobberSets(lvns);

  MergeNullChecked(lvns);  // May later insert more.
  MergeNonAliasingIFieldValues(lvns);
  MergeSFieldValues(lvns);

  // Quick hack relying on the other Merge() that I want to rewrite!
  for (const LocalValueNumbering* other : lvns) {
    if (other == lvns[0]) {
      global_memory_version_ = other->global_memory_version_;
      aliasing_ifield_version_map_ = other->aliasing_ifield_version_map_;
      non_aliasing_array_version_map_ = other->non_aliasing_array_version_map_;
      escaped_array_clobber_set_ = other->escaped_array_clobber_set_;
      std::copy(other->unresolved_ifield_version_, other->unresolved_ifield_version_ + kFieldTypeCount,
                unresolved_ifield_version_);
      std::copy(other->unresolved_sfield_version_, other->unresolved_sfield_version_ + kFieldTypeCount,
                unresolved_sfield_version_);
      std::copy(other->aliasing_array_version_, other->aliasing_array_version_ + kFieldTypeCount,
                aliasing_array_version_);
    } else {
      Merge(*other);
    }
  }
}

void LocalValueNumbering::Merge(const LocalValueNumbering& other) {
  // Merge local maps.
  MergeLocalMap(&aliasing_ifield_version_map_, other.aliasing_ifield_version_map_,
                kMergeBlockAliasingIFieldVersionBumpOp);
  MergeLocalMap(&non_aliasing_array_version_map_, other.non_aliasing_array_version_map_,
                kMergeBlockNonAliasingArrayVersionBumpOp);

  // Merge escaped references and clobber sets.
  if (global_memory_version_ != other.global_memory_version_) {
    // All fields or array elements of escaped references may have been modified.
    escaped_array_clobber_set_.clear();
  } else {
    MergeClobberSet(&escaped_array_clobber_set_, other. escaped_array_clobber_set_);
  }

  // Check memory version mismatch.
  uint16_t new_version = kNoValue;
  if (!SameMemoryVersion(other)) {
    new_version = gvn_->LookupValue(kMergeBlockMemoryVersionBumpOp, 0u, 0u, id_);
    if (global_memory_version_ != other.global_memory_version_) {
      global_memory_version_ = new_version;
      // Reset all aliasing memory versions.
      std::fill_n(unresolved_sfield_version_, kFieldTypeCount, new_version);
      std::fill_n(unresolved_ifield_version_, kFieldTypeCount, new_version);
      std::fill_n(aliasing_array_version_, kFieldTypeCount, new_version);
    } else {
      for (uint16_t type = 0; type != kFieldTypeCount; ++type)  {
        if (unresolved_ifield_version_[type] != other.unresolved_ifield_version_[type]) {
          unresolved_ifield_version_[type] = new_version;
        }
        if (unresolved_sfield_version_[type] != other.unresolved_sfield_version_[type]) {
          unresolved_sfield_version_[type] = new_version;
          RemoveSFieldsForType(type);
        }
        if (aliasing_array_version_[type] != other.aliasing_array_version_[type]) {
          aliasing_array_version_[type] = new_version;
        }
      }
    }
  }
}

uint16_t LocalValueNumbering::MarkNonAliasingNonNull(MIR* mir) {
  uint16_t res = gvn_->GetOperandValue(mir->ssa_rep->defs[0]);
  gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
  DCHECK(null_checked_.find(res) == null_checked_.end());
  null_checked_.insert(res);
  non_aliasing_refs_.insert(res);
  return res;
}

bool LocalValueNumbering::IsNonAliasing(uint16_t reg) {
  return non_aliasing_refs_.find(reg) != non_aliasing_refs_.end();
}

bool LocalValueNumbering::IsNonAliasingIField(uint16_t reg, uint16_t field_id, uint16_t type) {
  if (IsNonAliasing(reg)) {
    return true;
  }
  if (escaped_refs_.find(reg) == escaped_refs_.end()) {
    return false;
  }
  // Check for IPUTs to unresolved fields.
  EscapedIFieldClobberKey key1 = { reg, type, kNoValue };
  if (escaped_ifield_clobber_set_.find(key1) != escaped_ifield_clobber_set_.end()) {
    return false;
  }
  // Check for aliased IPUTs to the same field.
  EscapedIFieldClobberKey key2 = { reg, type, field_id };
  return escaped_ifield_clobber_set_.find(key2) == escaped_ifield_clobber_set_.end();
}

bool LocalValueNumbering::IsNonAliasingArray(uint16_t reg, uint16_t type) {
  if (IsNonAliasing(reg)) {
    return true;
  }
  if (escaped_refs_.count(reg) == 0u) {
    return false;
  }
  // Check for aliased APUTs.
  EscapedArrayClobberKey key = { reg, type };
  return escaped_array_clobber_set_.find(key) == escaped_array_clobber_set_.end();
}

void LocalValueNumbering::HandleNullCheck(MIR* mir, uint16_t reg) {
  auto lb = null_checked_.lower_bound(reg);
  if (lb != null_checked_.end() && *lb == reg) {
    if (LIKELY(gvn_->CanModify())) {
      if (gvn_->GetCompilationUnit()->verbose) {
        LOG(INFO) << "Removing null check for 0x" << std::hex << mir->offset;
      }
      mir->optimization_flags |= MIR_IGNORE_NULL_CHECK;
    }
  } else {
    null_checked_.insert(lb, reg);
  }
}

void LocalValueNumbering::HandleRangeCheck(MIR* mir, uint16_t array, uint16_t index) {
  RangeCheckKey key = { array, index };
  auto lb = range_checked_.lower_bound(key);
  if (lb != range_checked_.end() && !RangeCheckKeyComparator()(key, *lb)) {
    if (LIKELY(gvn_->CanModify())) {
      if (gvn_->GetCompilationUnit()->verbose) {
        LOG(INFO) << "Removing range check for 0x" << std::hex << mir->offset;
      }
      mir->optimization_flags |= MIR_IGNORE_RANGE_CHECK;
    }
  } else {
    // Mark range check completed.
    range_checked_.insert(lb, key);
  }
}

void LocalValueNumbering::HandlePutObject(MIR* mir) {
  // If we're storing a non-aliasing reference, stop tracking it as non-aliasing now.
  uint16_t base = gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
  HandleEscapingRef(base);
}

void LocalValueNumbering::HandleEscapingRef(uint16_t base) {
  auto it = non_aliasing_refs_.find(base);
  if (it != non_aliasing_refs_.end()) {
    non_aliasing_refs_.erase(it);
    escaped_refs_.insert(base);
  }
}

uint16_t LocalValueNumbering::HandleAGet(MIR* mir, uint16_t opcode) {
  // uint16_t type = opcode - Instruction::AGET;
  uint16_t array = gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
  HandleNullCheck(mir, array);
  uint16_t index = gvn_->GetOperandValue(mir->ssa_rep->uses[1]);
  HandleRangeCheck(mir, array, index);
  uint16_t type = opcode - Instruction::AGET;
  // Establish value number for loaded register.
  uint16_t res;
  if (IsNonAliasingArray(array, type)) {
    // Get the start version that accounts for aliasing within the array (different index names).
    uint16_t start_version =
        gvn_->LookupValue(kNonAliasingArrayStartVersionOp, array, kNoValue, kNoValue);
    // Find the current version from the non_aliasing_array_version_map_.
    uint16_t memory_version = start_version;
    auto it = non_aliasing_array_version_map_.find(start_version);
    if (it != non_aliasing_array_version_map_.end()) {
      memory_version = it->second;
    }
    res = gvn_->LookupValue(kNonAliasingArrayOp, array, index, memory_version);
  } else {
    // Get the memory version of aliased array accesses of this type.
    uint16_t memory_version = gvn_->LookupValue(kAliasingArrayMemoryVersionOp,
                                                global_memory_version_,
                                                aliasing_array_version_[type], kNoValue);
    res = gvn_->LookupValue(kAliasingArrayOp, array, index, memory_version);
  }
  if (opcode == Instruction::AGET_WIDE) {
    gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
  } else {
    gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
  }
  return res;
}

void LocalValueNumbering::HandleAPut(MIR* mir, uint16_t opcode) {
  int array_idx = (opcode == Instruction::APUT_WIDE) ? 2 : 1;
  int index_idx = array_idx + 1;
  uint16_t array = gvn_->GetOperandValue(mir->ssa_rep->uses[array_idx]);
  HandleNullCheck(mir, array);
  uint16_t index = gvn_->GetOperandValue(mir->ssa_rep->uses[index_idx]);
  HandleRangeCheck(mir, array, index);

  uint16_t type = opcode - Instruction::APUT;
  uint16_t value = (opcode == Instruction::APUT_WIDE)
                   ? gvn_->GetOperandValueWide(mir->ssa_rep->uses[0])
                   : gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
  if (IsNonAliasing(array)) {
    // Get the start version that accounts for aliasing within the array (different index values).
    uint16_t start_version =
        gvn_->LookupValue(kNonAliasingArrayStartVersionOp, array, kNoValue, kNoValue);
    auto it = non_aliasing_array_version_map_.find(start_version);
    uint16_t memory_version = start_version;
    if (it != non_aliasing_array_version_map_.end()) {
      memory_version = it->second;
    }
    if (gvn_->HasValue(kNonAliasingArrayOp, array, index, memory_version, value)) {
      // This APUT can be eliminated, it stores the same value that's already in the field.
      // TODO: Eliminate the APUT.
      return;
    }
    // We need to take 4 values (array, index, memory_version, value) into account for bumping
    // the memory version but the key can take only 3. Merge array and index into a location.
    uint16_t array_access_location = gvn_->LookupValue(kArrayAccessLocOp, array, index, kNoValue);
    // Bump the version, adding to the chain.
    memory_version = gvn_->LookupValue(kAliasingArrayBumpVersionOp, memory_version,
                                       array_access_location, value);
    non_aliasing_array_version_map_.Overwrite(start_version, memory_version);
    gvn_->StoreValue(kNonAliasingArrayOp, array, index, memory_version, value);
  } else {
    // Get the memory version based on global_memory_version_ and aliasing_array_version_[type].
    uint16_t memory_version = gvn_->LookupValue(kAliasingArrayMemoryVersionOp,
                                                global_memory_version_,
                                                aliasing_array_version_[type], kNoValue);
    if (gvn_->HasValue(kAliasingArrayOp, array, index, memory_version, value)) {
      // This APUT can be eliminated, it stores the same value that's already in the field.
      // TODO: Eliminate the APUT.
      return;
    }
    // We need to take 4 values (array, index, memory_version, value) into account for bumping
    // the memory version but the key can take only 3. Merge array and index into a location.
    uint16_t array_access_location = gvn_->LookupValue(kArrayAccessLocOp, array, index, kNoValue);
    // Bump the version, adding to the chain.
    uint16_t bumped_version = gvn_->LookupValue(kAliasingArrayBumpVersionOp, memory_version,
                                                array_access_location, value);
    aliasing_array_version_[type] = bumped_version;
    memory_version = gvn_->LookupValue(kAliasingArrayMemoryVersionOp, global_memory_version_,
                                       bumped_version, kNoValue);
    gvn_->StoreValue(kAliasingArrayOp, array, index, memory_version, value);

    // Clobber all escaped array refs for this type.
    for (uint16_t escaped_array : escaped_refs_) {
      EscapedArrayClobberKey clobber_key = { escaped_array, type };
      escaped_array_clobber_set_.insert(clobber_key);
    }
  }
}

uint16_t LocalValueNumbering::HandleIGet(MIR* mir, uint16_t opcode) {
  uint16_t base = gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
  HandleNullCheck(mir, base);
  const MirFieldInfo& field_info = gvn_->GetMirGraph()->GetIFieldLoweringInfo(mir);
  uint16_t res;
  if (!field_info.IsResolved() || field_info.IsVolatile()) {
    // Volatile fields always get a new memory version; field id is irrelevant.
    // Unresolved fields may be volatile, so handle them as such to be safe.
    // Use result s_reg - will be unique.
    res = gvn_->LookupValue(kNoValue, mir->ssa_rep->defs[0], kNoValue, kNoValue);
  } else {
    uint16_t type = opcode - Instruction::IGET;
    uint16_t field_id = gvn_->GetFieldId(field_info, type);
    if (IsNonAliasingIField(base, field_id, type)) {
      uint16_t loc = gvn_->LookupValue(kNonAliasingIFieldLocOp, base, field_id, type);
      auto lb = non_aliasing_ifield_value_map_.lower_bound(loc);
      if (lb != non_aliasing_ifield_value_map_.end() && lb->first == loc) {
        res = lb->second;
      } else {
        res = gvn_->LookupValue(kNonAliasingIFieldInitialOp, loc, kNoValue, kNoValue);
        non_aliasing_ifield_value_map_.PutHint(lb, loc, res);
      }
    } else {
      // Get the start version that accounts for aliasing with unresolved fields of the same type
      // and make it unique for the field by including the field_id.
      uint16_t start_version = gvn_->LookupValue(kAliasingIFieldStartVersionOp,
                                                 global_memory_version_,
                                                 unresolved_ifield_version_[type], field_id);
      // Find the current version from the aliasing_ifield_version_map_.
      uint16_t memory_version = start_version;
      auto version_lb = aliasing_ifield_version_map_.lower_bound(start_version);
      if (version_lb != aliasing_ifield_version_map_.end() && version_lb->first == start_version) {
        memory_version = version_lb->second;
      }
      res = gvn_->LookupValue(kAliasingIFieldOp, base, field_id, memory_version);
    }
  }
  if (opcode == Instruction::IGET_WIDE) {
    gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
  } else {
    gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
  }
  return res;
}

void LocalValueNumbering::HandleIPut(MIR* mir, uint16_t opcode) {
  uint16_t type = opcode - Instruction::IPUT;
  int base_reg = (opcode == Instruction::IPUT_WIDE) ? 2 : 1;
  uint16_t base = gvn_->GetOperandValue(mir->ssa_rep->uses[base_reg]);
  HandleNullCheck(mir, base);
  const MirFieldInfo& field_info = gvn_->GetMirGraph()->GetIFieldLoweringInfo(mir);
  if (!field_info.IsResolved()) {
    // Unresolved fields always alias with everything of the same type.
    // Use mir->offset as modifier; without elaborate inlining, it will be unique.
    unresolved_ifield_version_[type] =
        gvn_->LookupValue(kUnresolvedIFieldOp, kNoValue, kNoValue, mir->offset);

    // For simplicity, treat base as escaped now.
    HandleEscapingRef(base);

    // Clobber all fields of escaped references of the same type.
    for (uint16_t escaped_ref : escaped_refs_) {
      EscapedIFieldClobberKey clobber_key = { escaped_ref, type, kNoValue };
      escaped_ifield_clobber_set_.insert(clobber_key);
    }
  } else if (field_info.IsVolatile()) {
    // Nothing to do, resolved volatile fields always get a new memory version anyway and
    // can't alias with resolved non-volatile fields.
  } else {
    uint16_t field_id = gvn_->GetFieldId(field_info, type);
    uint16_t value = (opcode == Instruction::IPUT_WIDE)
                     ? gvn_->GetOperandValueWide(mir->ssa_rep->uses[0])
                     : gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
    if (IsNonAliasing(base)) {
      uint16_t loc = gvn_->LookupValue(kNonAliasingIFieldLocOp, base, field_id, type);
      auto lb = non_aliasing_ifield_value_map_.lower_bound(loc);
      if (lb != non_aliasing_ifield_value_map_.end() && lb->first == loc) {
        if (lb->second == value) {
          // This IPUT can be eliminated, it stores the same value that's already in the field.
          // TODO: Eliminate the IPUT.
          return;
        }
        lb->second = value;  // Overwrite.
      } else {
        non_aliasing_ifield_value_map_.PutHint(lb, loc, value);
      }
    } else {
      // Get the start version that accounts for aliasing with unresolved fields of the same type
      // and make it unique for the field by including the field_id.
      uint16_t start_version = gvn_->LookupValue(kAliasingIFieldStartVersionOp,
                                                 global_memory_version_,
                                                 unresolved_ifield_version_[type], field_id);
      // Find the old version from the aliasing_ifield_version_map_.
      uint16_t old_version = start_version;
      auto version_it = aliasing_ifield_version_map_.find(start_version);
      if (version_it != aliasing_ifield_version_map_.end()) {
        old_version = version_it->second;
      }
      // Check if the field currently contains the value, making this a NOP.
      if (gvn_->HasValue(kAliasingIFieldOp, base, field_id, old_version, value)) {
        // This IPUT can be eliminated, it stores the same value that's already in the field.
        // TODO: Eliminate the IPUT.
        return;
      }
      // Bump the version, adding to the chain started by start_version.
      uint16_t memory_version =
          gvn_->LookupValue(kAliasingIFieldBumpVersionOp, old_version, base, value);
      // Update the aliasing_ifield_version_map_ so that HandleIGet() can get the memory_version
      // without knowing the values used to build the chain.
      aliasing_ifield_version_map_.Overwrite(start_version, memory_version);
      gvn_->StoreValue(kAliasingIFieldOp, base, field_id, memory_version, value);

      // Clobber all fields of escaped references for this field.
      for (uint16_t escaped_ref : escaped_refs_) {
        EscapedIFieldClobberKey clobber_key = { escaped_ref, type, field_id };
        escaped_ifield_clobber_set_.insert(clobber_key);
      }
    }
  }
}

uint16_t LocalValueNumbering::HandleSGet(MIR* mir, uint16_t opcode) {
  const MirFieldInfo& field_info = gvn_->GetMirGraph()->GetSFieldLoweringInfo(mir);
  uint16_t res;
  if (!field_info.IsResolved() || field_info.IsVolatile()) {
    // Volatile fields always get a new memory version; field id is irrelevant.
    // Unresolved fields may be volatile, so handle them as such to be safe.
    // Use result s_reg - will be unique.
    res = gvn_->LookupValue(kNoValue, mir->ssa_rep->defs[0], kNoValue, kNoValue);
  } else {
    uint16_t type = opcode - Instruction::SGET;
    uint16_t field_id = gvn_->GetFieldId(field_info, type);
    auto lb = sfield_value_map_.lower_bound(field_id);
    if (lb != sfield_value_map_.end() && lb->first == field_id) {
      res = lb->second;
    } else {
      // Resolved non-volatile static fields can alias with non-resolved fields of the same type,
      // so we need to use unresolved_sfield_version_[type] in addition to global_memory_version_
      // to determine the version of the field.
      res = gvn_->LookupValue(kResolvedSFieldOp, field_id,
                              unresolved_sfield_version_[type], global_memory_version_);
      sfield_value_map_.PutHint(lb, field_id, res);
    }
  }
  if (opcode == Instruction::SGET_WIDE) {
    gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
  } else {
    gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
  }
  return res;
}

void LocalValueNumbering::HandleSPut(MIR* mir, uint16_t opcode) {
  uint16_t type = opcode - Instruction::SPUT;
  const MirFieldInfo& field_info = gvn_->GetMirGraph()->GetSFieldLoweringInfo(mir);
  if (!field_info.IsResolved()) {
    // Unresolved fields always alias with everything of the same type.
    // Use mir->offset as modifier; without elaborate inlining, it will be unique.
    unresolved_sfield_version_[type] =
        gvn_->LookupValue(kUnresolvedSFieldOp, kNoValue, kNoValue, mir->offset);
    RemoveSFieldsForType(type);
  } else if (field_info.IsVolatile()) {
    // Nothing to do, resolved volatile fields always get a new memory version anyway and
    // can't alias with resolved non-volatile fields.
  } else {
    uint16_t field_id = gvn_->GetFieldId(field_info, type);
    uint16_t value = (opcode == Instruction::SPUT_WIDE)
                     ? gvn_->GetOperandValueWide(mir->ssa_rep->uses[0])
                     : gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
    // Resolved non-volatile static fields can alias with non-resolved fields of the same type,
    // so we need to use unresolved_sfield_version_[type] in addition to global_memory_version_
    // to determine the version of the field.
    auto lb = sfield_value_map_.lower_bound(field_id);
    if (lb != sfield_value_map_.end() && lb->first == field_id) {
      if (lb->second == value) {
        // This SPUT can be eliminated, it stores the same value that's already in the field.
        // TODO: Eliminate the SPUT.
        return;
      }
      lb->second = value;  // Overwrite.
    } else {
      sfield_value_map_.PutHint(lb, field_id, value);
    }
  }
}

void LocalValueNumbering::RemoveSFieldsForType(uint16_t type) {
  // Erase all static fields of this type from the sfield_value_map_.
  for (auto it = sfield_value_map_.begin(), end = sfield_value_map_.end(); it != end; ) {
    if (gvn_->GetFieldType(it->first) == type) {
      it = sfield_value_map_.erase(it);
    } else {
      ++it;
    }
  }
}

uint16_t LocalValueNumbering::GetValueNumber(MIR* mir) {
  uint16_t res = kNoValue;
  uint16_t opcode = mir->dalvikInsn.opcode;
  switch (opcode) {
    case Instruction::NOP:
    case Instruction::RETURN_VOID:
    case Instruction::RETURN:
    case Instruction::RETURN_OBJECT:
    case Instruction::RETURN_WIDE:
    case Instruction::MONITOR_ENTER:
    case Instruction::MONITOR_EXIT:
    case Instruction::GOTO:
    case Instruction::GOTO_16:
    case Instruction::GOTO_32:
    case Instruction::CHECK_CAST:
    case Instruction::THROW:
    case Instruction::FILL_ARRAY_DATA:
    case Instruction::PACKED_SWITCH:
    case Instruction::SPARSE_SWITCH:
    case Instruction::IF_EQ:
    case Instruction::IF_NE:
    case Instruction::IF_LT:
    case Instruction::IF_GE:
    case Instruction::IF_GT:
    case Instruction::IF_LE:
    case Instruction::IF_EQZ:
    case Instruction::IF_NEZ:
    case Instruction::IF_LTZ:
    case Instruction::IF_GEZ:
    case Instruction::IF_GTZ:
    case Instruction::IF_LEZ:
    case kMirOpFusedCmplFloat:
    case kMirOpFusedCmpgFloat:
    case kMirOpFusedCmplDouble:
    case kMirOpFusedCmpgDouble:
    case kMirOpFusedCmpLong:
      // Nothing defined - take no action.
      break;

    case Instruction::FILLED_NEW_ARRAY:
    case Instruction::FILLED_NEW_ARRAY_RANGE:
      // Nothing defined but the result will be unique and non-null.
      if (mir->next != nullptr && mir->next->dalvikInsn.opcode == Instruction::MOVE_RESULT_OBJECT) {
        uint16_t array = MarkNonAliasingNonNull(mir->next);
        if (kLocalValueNumberingEnableFilledNewArrayTracking && mir->ssa_rep->num_uses != 0u) {
          uint16_t memory_version =
              gvn_->LookupValue(kNonAliasingArrayStartVersionOp, array, kNoValue, kNoValue);
          DCHECK_EQ(non_aliasing_array_version_map_.count(memory_version), 0u);
          for (size_t i = 0u, count = mir->ssa_rep->num_uses; i != count; ++i) {
            DCHECK_EQ(High16Bits(i), 0u);
            uint16_t index = gvn_->LookupValue(Instruction::CONST, i, 0u, 0);
            uint16_t value = gvn_->GetOperandValue(mir->ssa_rep->uses[i]);
            gvn_->StoreValue(kNonAliasingArrayOp, array, index, memory_version, value);
            RangeCheckKey key = { array, index };
            range_checked_.insert(key);
          }
        }
        // TUNING: We could track value names stored in the array.
        // The MOVE_RESULT_OBJECT will be processed next and we'll return the value name then.
      }
      // All args escaped (if references).
      for (size_t i = 0u, count = mir->ssa_rep->num_uses; i != count; ++i) {
        uint16_t reg = gvn_->GetOperandValue(mir->ssa_rep->uses[i]);
        HandleEscapingRef(reg);
      }
      break;

    case Instruction::INVOKE_DIRECT:
    case Instruction::INVOKE_DIRECT_RANGE:
    case Instruction::INVOKE_VIRTUAL:
    case Instruction::INVOKE_VIRTUAL_RANGE:
    case Instruction::INVOKE_SUPER:
    case Instruction::INVOKE_SUPER_RANGE:
    case Instruction::INVOKE_INTERFACE:
    case Instruction::INVOKE_INTERFACE_RANGE: {
        // Nothing defined but handle the null check.
        uint16_t reg = gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
        HandleNullCheck(mir, reg);
      }
      // Intentional fall-through.
    case Instruction::INVOKE_STATIC:
    case Instruction::INVOKE_STATIC_RANGE:
      if ((mir->optimization_flags & MIR_INLINED) == 0) {
        // Use mir->offset as modifier; without elaborate inlining, it will be unique.
        global_memory_version_ =
            gvn_->LookupValue(kInvokeMemoryVersionBumpOp, 0u, 0u, mir->offset);
        // Make ref args aliasing.
        for (size_t i = 0u, count = mir->ssa_rep->num_uses; i != count; ++i) {
          uint16_t reg = gvn_->GetOperandValue(mir->ssa_rep->uses[i]);
          non_aliasing_refs_.erase(reg);
        }
        // All static fields may have been modified.
        sfield_value_map_.clear();
        // All fields or array elements of escaped references may have been modified.
        escaped_refs_.clear();
        escaped_ifield_clobber_set_.clear();
        escaped_array_clobber_set_.clear();
      }
      break;

    case Instruction::MOVE_RESULT:
    case Instruction::MOVE_RESULT_OBJECT:
    case Instruction::INSTANCE_OF:
      // 1 result, treat as unique each time, use result s_reg - will be unique.
      res = gvn_->GetOperandValue(mir->ssa_rep->defs[0]);
      gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      break;
    case Instruction::MOVE_EXCEPTION:
    case Instruction::NEW_INSTANCE:
    case Instruction::CONST_CLASS:
    case Instruction::NEW_ARRAY:
      // 1 result, treat as unique each time, use result s_reg - will be unique.
      res = MarkNonAliasingNonNull(mir);
      break;
    case Instruction::CONST_STRING:
    case Instruction::CONST_STRING_JUMBO:
      // These strings are internalized, so assign value based on the string pool index.
      res = gvn_->LookupValue(Instruction::CONST_STRING, Low16Bits(mir->dalvikInsn.vB),
                              High16Bits(mir->dalvikInsn.vB), 0);
      gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      null_checked_.insert(res);  // May already be there.
      // NOTE: Hacking the contents of an internalized string via reflection is possible
      // but the behavior is undefined. Therefore, we consider the string constant and
      // the reference non-aliasing.
      // TUNING: We could keep this property even if the reference "escapes".
      non_aliasing_refs_.insert(res);  // May already be there.
      break;
    case Instruction::MOVE_RESULT_WIDE:
      // 1 wide result, treat as unique each time, use result s_reg - will be unique.
      res = gvn_->GetOperandValueWide(mir->ssa_rep->defs[0]);
      gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
      break;

    case kMirOpPhi:
      // TODO: Shifting to GVN. Reuse the same value name if the Phi has already been seen
      // somewhere in memory. Otherwise, check if all incoming sregs are null-checked.
      break;

    case Instruction::MOVE:
    case Instruction::MOVE_OBJECT:
    case Instruction::MOVE_16:
    case Instruction::MOVE_OBJECT_16:
    case Instruction::MOVE_FROM16:
    case Instruction::MOVE_OBJECT_FROM16:
    case kMirOpCopy:
      // Just copy value number of source to value number of result.
      res = gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
      gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      break;

    case Instruction::MOVE_WIDE:
    case Instruction::MOVE_WIDE_16:
    case Instruction::MOVE_WIDE_FROM16:
      // Just copy value number of source to value number of result.
      res = gvn_->GetOperandValueWide(mir->ssa_rep->uses[0]);
      gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
      break;

    case Instruction::CONST:
    case Instruction::CONST_4:
    case Instruction::CONST_16:
      res = gvn_->LookupValue(Instruction::CONST, Low16Bits(mir->dalvikInsn.vB),
                              High16Bits(mir->dalvikInsn.vB), 0);
      gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      break;

    case Instruction::CONST_HIGH16:
      res = gvn_->LookupValue(Instruction::CONST, 0, mir->dalvikInsn.vB, 0);
      gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      break;

    case Instruction::CONST_WIDE_16:
    case Instruction::CONST_WIDE_32: {
        uint16_t low_res = gvn_->LookupValue(Instruction::CONST, Low16Bits(mir->dalvikInsn.vB),
                                             High16Bits(mir->dalvikInsn.vB >> 16), 1);
        uint16_t high_res;
        if (mir->dalvikInsn.vB & 0x80000000) {
          high_res = gvn_->LookupValue(Instruction::CONST, 0xffff, 0xffff, 2);
        } else {
          high_res = gvn_->LookupValue(Instruction::CONST, 0, 0, 2);
        }
        res = gvn_->LookupValue(Instruction::CONST, low_res, high_res, 3);
        gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::CONST_WIDE: {
        uint32_t low_word = Low32Bits(mir->dalvikInsn.vB_wide);
        uint32_t high_word = High32Bits(mir->dalvikInsn.vB_wide);
        uint16_t low_res = gvn_->LookupValue(Instruction::CONST, Low16Bits(low_word),
                                             High16Bits(low_word), 1);
        uint16_t high_res = gvn_->LookupValue(Instruction::CONST, Low16Bits(high_word),
                                              High16Bits(high_word), 2);
        res = gvn_->LookupValue(Instruction::CONST, low_res, high_res, 3);
        gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::CONST_WIDE_HIGH16: {
        uint16_t low_res = gvn_->LookupValue(Instruction::CONST, 0, 0, 1);
        uint16_t high_res = gvn_->LookupValue(Instruction::CONST, 0,
                                              Low16Bits(mir->dalvikInsn.vB), 2);
        res = gvn_->LookupValue(Instruction::CONST, low_res, high_res, 3);
        gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::ARRAY_LENGTH:
    case Instruction::NEG_INT:
    case Instruction::NOT_INT:
    case Instruction::NEG_FLOAT:
    case Instruction::INT_TO_BYTE:
    case Instruction::INT_TO_SHORT:
    case Instruction::INT_TO_CHAR:
    case Instruction::INT_TO_FLOAT:
    case Instruction::FLOAT_TO_INT: {
        // res = op + 1 operand
        uint16_t operand1 = gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
        res = gvn_->LookupValue(opcode, operand1, kNoValue, kNoValue);
        gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::LONG_TO_FLOAT:
    case Instruction::LONG_TO_INT:
    case Instruction::DOUBLE_TO_FLOAT:
    case Instruction::DOUBLE_TO_INT: {
        // res = op + 1 wide operand
        uint16_t operand1 = gvn_->GetOperandValueWide(mir->ssa_rep->uses[0]);
        res = gvn_->LookupValue(opcode, operand1, kNoValue, kNoValue);
        gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      }
      break;


    case Instruction::DOUBLE_TO_LONG:
    case Instruction::LONG_TO_DOUBLE:
    case Instruction::NEG_LONG:
    case Instruction::NOT_LONG:
    case Instruction::NEG_DOUBLE: {
        // wide res = op + 1 wide operand
        uint16_t operand1 = gvn_->GetOperandValueWide(mir->ssa_rep->uses[0]);
        res = gvn_->LookupValue(opcode, operand1, kNoValue, kNoValue);
        gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::FLOAT_TO_DOUBLE:
    case Instruction::FLOAT_TO_LONG:
    case Instruction::INT_TO_DOUBLE:
    case Instruction::INT_TO_LONG: {
        // wide res = op + 1 operand
        uint16_t operand1 = gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
        res = gvn_->LookupValue(opcode, operand1, kNoValue, kNoValue);
        gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::CMPL_DOUBLE:
    case Instruction::CMPG_DOUBLE:
    case Instruction::CMP_LONG: {
        // res = op + 2 wide operands
        uint16_t operand1 = gvn_->GetOperandValueWide(mir->ssa_rep->uses[0]);
        uint16_t operand2 = gvn_->GetOperandValueWide(mir->ssa_rep->uses[2]);
        res = gvn_->LookupValue(opcode, operand1, operand2, kNoValue);
        gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::CMPG_FLOAT:
    case Instruction::CMPL_FLOAT:
    case Instruction::ADD_INT:
    case Instruction::ADD_INT_2ADDR:
    case Instruction::MUL_INT:
    case Instruction::MUL_INT_2ADDR:
    case Instruction::AND_INT:
    case Instruction::AND_INT_2ADDR:
    case Instruction::OR_INT:
    case Instruction::OR_INT_2ADDR:
    case Instruction::XOR_INT:
    case Instruction::XOR_INT_2ADDR:
    case Instruction::SUB_INT:
    case Instruction::SUB_INT_2ADDR:
    case Instruction::DIV_INT:
    case Instruction::DIV_INT_2ADDR:
    case Instruction::REM_INT:
    case Instruction::REM_INT_2ADDR:
    case Instruction::SHL_INT:
    case Instruction::SHL_INT_2ADDR:
    case Instruction::SHR_INT:
    case Instruction::SHR_INT_2ADDR:
    case Instruction::USHR_INT:
    case Instruction::USHR_INT_2ADDR: {
        // res = op + 2 operands
        uint16_t operand1 = gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
        uint16_t operand2 = gvn_->GetOperandValue(mir->ssa_rep->uses[1]);
        res = gvn_->LookupValue(opcode, operand1, operand2, kNoValue);
        gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::ADD_LONG:
    case Instruction::SUB_LONG:
    case Instruction::MUL_LONG:
    case Instruction::DIV_LONG:
    case Instruction::REM_LONG:
    case Instruction::AND_LONG:
    case Instruction::OR_LONG:
    case Instruction::XOR_LONG:
    case Instruction::ADD_LONG_2ADDR:
    case Instruction::SUB_LONG_2ADDR:
    case Instruction::MUL_LONG_2ADDR:
    case Instruction::DIV_LONG_2ADDR:
    case Instruction::REM_LONG_2ADDR:
    case Instruction::AND_LONG_2ADDR:
    case Instruction::OR_LONG_2ADDR:
    case Instruction::XOR_LONG_2ADDR:
    case Instruction::ADD_DOUBLE:
    case Instruction::SUB_DOUBLE:
    case Instruction::MUL_DOUBLE:
    case Instruction::DIV_DOUBLE:
    case Instruction::REM_DOUBLE:
    case Instruction::ADD_DOUBLE_2ADDR:
    case Instruction::SUB_DOUBLE_2ADDR:
    case Instruction::MUL_DOUBLE_2ADDR:
    case Instruction::DIV_DOUBLE_2ADDR:
    case Instruction::REM_DOUBLE_2ADDR: {
        // wide res = op + 2 wide operands
        uint16_t operand1 = gvn_->GetOperandValueWide(mir->ssa_rep->uses[0]);
        uint16_t operand2 = gvn_->GetOperandValueWide(mir->ssa_rep->uses[2]);
        res = gvn_->LookupValue(opcode, operand1, operand2, kNoValue);
        gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::SHL_LONG:
    case Instruction::SHR_LONG:
    case Instruction::USHR_LONG:
    case Instruction::SHL_LONG_2ADDR:
    case Instruction::SHR_LONG_2ADDR:
    case Instruction::USHR_LONG_2ADDR: {
        // wide res = op + 1 wide operand + 1 operand
        uint16_t operand1 = gvn_->GetOperandValueWide(mir->ssa_rep->uses[0]);
        uint16_t operand2 = gvn_->GetOperandValue(mir->ssa_rep->uses[2]);
        res = gvn_->LookupValue(opcode, operand1, operand2, kNoValue);
        gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::ADD_FLOAT:
    case Instruction::SUB_FLOAT:
    case Instruction::MUL_FLOAT:
    case Instruction::DIV_FLOAT:
    case Instruction::REM_FLOAT:
    case Instruction::ADD_FLOAT_2ADDR:
    case Instruction::SUB_FLOAT_2ADDR:
    case Instruction::MUL_FLOAT_2ADDR:
    case Instruction::DIV_FLOAT_2ADDR:
    case Instruction::REM_FLOAT_2ADDR: {
        // res = op + 2 operands
        uint16_t operand1 = gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
        uint16_t operand2 = gvn_->GetOperandValue(mir->ssa_rep->uses[1]);
        res = gvn_->LookupValue(opcode, operand1, operand2, kNoValue);
        gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::RSUB_INT:
    case Instruction::ADD_INT_LIT16:
    case Instruction::MUL_INT_LIT16:
    case Instruction::DIV_INT_LIT16:
    case Instruction::REM_INT_LIT16:
    case Instruction::AND_INT_LIT16:
    case Instruction::OR_INT_LIT16:
    case Instruction::XOR_INT_LIT16:
    case Instruction::ADD_INT_LIT8:
    case Instruction::RSUB_INT_LIT8:
    case Instruction::MUL_INT_LIT8:
    case Instruction::DIV_INT_LIT8:
    case Instruction::REM_INT_LIT8:
    case Instruction::AND_INT_LIT8:
    case Instruction::OR_INT_LIT8:
    case Instruction::XOR_INT_LIT8:
    case Instruction::SHL_INT_LIT8:
    case Instruction::SHR_INT_LIT8:
    case Instruction::USHR_INT_LIT8: {
        // Same as res = op + 2 operands, except use vC as operand 2
        uint16_t operand1 = gvn_->GetOperandValue(mir->ssa_rep->uses[0]);
        uint16_t operand2 = gvn_->LookupValue(Instruction::CONST, mir->dalvikInsn.vC, 0, 0);
        res = gvn_->LookupValue(opcode, operand1, operand2, kNoValue);
        gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
      }
      break;

    case Instruction::AGET_OBJECT:
    case Instruction::AGET:
    case Instruction::AGET_WIDE:
    case Instruction::AGET_BOOLEAN:
    case Instruction::AGET_BYTE:
    case Instruction::AGET_CHAR:
    case Instruction::AGET_SHORT:
      res = HandleAGet(mir, opcode);
      break;

    case Instruction::APUT_OBJECT:
      HandlePutObject(mir);
      // Intentional fall-through.
    case Instruction::APUT:
    case Instruction::APUT_WIDE:
    case Instruction::APUT_BYTE:
    case Instruction::APUT_BOOLEAN:
    case Instruction::APUT_SHORT:
    case Instruction::APUT_CHAR:
      HandleAPut(mir, opcode);
      break;

    case Instruction::IGET_OBJECT:
    case Instruction::IGET:
    case Instruction::IGET_WIDE:
    case Instruction::IGET_BOOLEAN:
    case Instruction::IGET_BYTE:
    case Instruction::IGET_CHAR:
    case Instruction::IGET_SHORT:
      res = HandleIGet(mir, opcode);
      break;

    case Instruction::IPUT_OBJECT:
      HandlePutObject(mir);
      // Intentional fall-through.
    case Instruction::IPUT:
    case Instruction::IPUT_WIDE:
    case Instruction::IPUT_BOOLEAN:
    case Instruction::IPUT_BYTE:
    case Instruction::IPUT_CHAR:
    case Instruction::IPUT_SHORT:
      HandleIPut(mir, opcode);
      break;

    case Instruction::SGET_OBJECT:
    case Instruction::SGET:
    case Instruction::SGET_WIDE:
    case Instruction::SGET_BOOLEAN:
    case Instruction::SGET_BYTE:
    case Instruction::SGET_CHAR:
    case Instruction::SGET_SHORT:
      res = HandleSGet(mir, opcode);
      break;

    case Instruction::SPUT_OBJECT:
      HandlePutObject(mir);
      // Intentional fall-through.
    case Instruction::SPUT:
    case Instruction::SPUT_WIDE:
    case Instruction::SPUT_BOOLEAN:
    case Instruction::SPUT_BYTE:
    case Instruction::SPUT_CHAR:
    case Instruction::SPUT_SHORT:
      HandleSPut(mir, opcode);
      break;
  }
  return res;
}

}    // namespace art

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
static constexpr uint16_t kMergeBlockMergeAliasingIFieldOp = Instruction::IPUT_WIDE;
static constexpr uint16_t kMergeBlockNonAliasingArrayVersionBumpOp = Instruction::APUT;
static constexpr uint16_t kMergeBlockNonAliasingIFieldVersionBumpOp = Instruction::APUT_WIDE;
static constexpr uint16_t kMergeBlockSFieldVersionBumpOp = Instruction::APUT_OBJECT;

}  // anonymous namespace

LocalValueNumbering::LocalValueNumbering(GlobalValueNumbering* gvn, uint16_t id)
    : gvn_(gvn),
      id_(id),
      sfield_value_map_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      non_aliasing_ifield_value_map_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      aliasing_ifield_value_map_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      global_memory_version_(0u),
      non_aliasing_array_version_map_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      non_aliasing_refs_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      escaped_refs_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      escaped_ifield_clobber_set_(EscapedIFieldClobberKeyComparator(), gvn->Allocator()->Adapter()),
      escaped_array_clobber_set_(EscapedArrayClobberKeyComparator(), gvn->Allocator()->Adapter()),
      range_checked_(RangeCheckKeyComparator() , gvn->Allocator()->Adapter()),
      null_checked_(std::less<uint16_t>(), gvn->Allocator()->Adapter()),
      merge_names_(gvn->Allocator()->Adapter()),
      merge_map_(std::less<ScopedArenaVector<BasicBlockId>>(), gvn->Allocator()->Adapter()),
      merge_new_memory_version_(kNoValue) {
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
      aliasing_ifield_value_map_ == other.aliasing_ifield_value_map_ &&
      sfield_value_map_ == other.sfield_value_map_ &&
      SameMemoryVersion(other) &&
      non_aliasing_array_version_map_ == other.non_aliasing_array_version_map_ &&
      non_aliasing_refs_ == other.non_aliasing_refs_ &&
      escaped_refs_ == other.escaped_refs_ &&
      escaped_ifield_clobber_set_ == other.escaped_ifield_clobber_set_ &&
      escaped_array_clobber_set_ == other.escaped_array_clobber_set_ &&
      range_checked_ == other.range_checked_ &&
      null_checked_ == other.null_checked_;
}

void LocalValueNumbering::Copy(const LocalValueNumbering& other) {
  non_aliasing_ifield_value_map_ = other.non_aliasing_ifield_value_map_;
  sfield_value_map_ = other.sfield_value_map_;
  global_memory_version_ = other.global_memory_version_;
  aliasing_ifield_value_map_ = other.aliasing_ifield_value_map_;
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

uint16_t LocalValueNumbering::NewMemoryVersion(uint16_t* new_version) {
  if (*new_version == kNoValue) {
    *new_version = gvn_->LookupValue(kMergeBlockMemoryVersionBumpOp, 0u, 0u, id_);
  }
  return *new_version;
}

void LocalValueNumbering::MergeMemoryVersions() {
  DCHECK_GE(gvn_->merge_lvns_.size(), 2u);
  const LocalValueNumbering* cmp = gvn_->merge_lvns_[0];
  // Check if the global version has changed.
  for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
    if (lvn->global_memory_version_ != cmp->global_memory_version_) {
      // Use a new version for everything.
      global_memory_version_ = NewMemoryVersion(&merge_new_memory_version_);
      std::fill_n(unresolved_sfield_version_, kFieldTypeCount, merge_new_memory_version_);
      std::fill_n(unresolved_ifield_version_, kFieldTypeCount, merge_new_memory_version_);
      std::fill_n(aliasing_array_version_, kFieldTypeCount, merge_new_memory_version_);
      return;
    }
  }
  // Initialize with a copy of memory versions from the comparison LVN.
  global_memory_version_ = cmp->global_memory_version_;
  std::copy_n(cmp->unresolved_ifield_version_, kFieldTypeCount, unresolved_ifield_version_);
  std::copy_n(cmp->unresolved_sfield_version_, kFieldTypeCount, unresolved_sfield_version_);
  std::copy_n(cmp->aliasing_array_version_, kFieldTypeCount, aliasing_array_version_);
  for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
    if (lvn == cmp) {
      continue;
    }
    for (size_t i = 0; i != kFieldTypeCount; ++i) {
      if (lvn->unresolved_ifield_version_[i] != cmp->unresolved_ifield_version_[i]) {
        unresolved_ifield_version_[i] = NewMemoryVersion(&merge_new_memory_version_);
      }
      if (lvn->unresolved_sfield_version_[i] != cmp->unresolved_sfield_version_[i]) {
        unresolved_sfield_version_[i] = NewMemoryVersion(&merge_new_memory_version_);
      }
      if (lvn->aliasing_array_version_[i] != cmp->aliasing_array_version_[i]) {
        aliasing_array_version_[i] = NewMemoryVersion(&merge_new_memory_version_);
      }
    }
  }
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

template <typename Set, Set LocalValueNumbering::* set_ptr>
void LocalValueNumbering::IntersectSets() {
  DCHECK_GE(gvn_->merge_lvns_.size(), 2u);

  // Find the LVN with the least entries in the set.
  const LocalValueNumbering* least_entries_lvn = gvn_->merge_lvns_[0];
  for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
    if ((lvn->*set_ptr).size() < (least_entries_lvn->*set_ptr).size()) {
      least_entries_lvn = lvn;
    }
  }

  // For each key check if it's in all the LVNs.
  for (const auto& key : least_entries_lvn->*set_ptr) {
    bool checked = true;
    for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
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

template <typename Set, Set LocalValueNumbering::*set_ptr, void (LocalValueNumbering::*MergeFn)(
    const typename Set::value_type& entry, typename Set::iterator hint)>
void LocalValueNumbering::MergeSets() {
  auto cmp = (this->*set_ptr).value_comp();
  for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
    auto my_it = (this->*set_ptr).begin(), my_end = (this->*set_ptr).end();
    for (const auto& entry : lvn->*set_ptr) {
      while (my_it != my_end && cmp(*my_it, entry)) {
        ++my_it;
      }
      if (my_it != my_end && !cmp(entry, *my_it)) {
        // Already handled.
        ++my_it;
      } else {
        // Merge values for this field_id.
        (this->*MergeFn)(entry, my_it);  // my_it remains valid across inserts to std::set/SafeMap.
      }
    }
  }
}

template <typename Map>
void LocalValueNumbering::InPlaceMapUnion(Map* work_map, const Map& other_map) {
  auto work_it = work_map->begin(), work_end = work_map->end();
  auto cmp = work_map->value_comp();
  for (const auto& entry : other_map) {
    while (work_it != work_end && cmp(*work_it, entry)) {
      ++work_it;
    }
    if (work_it != work_end && !cmp(entry, *work_it)) {
      // Already present. Check that the values are the same. (Must be equality-comparable.)
      DCHECK(entry.second == work_it->second);
    } else {
      work_map->PutHint(work_it, entry.first, entry.second);
    }
  }
}

void LocalValueNumbering::MergeAliasingIFieldValueRefs(AliasingIFieldValues* work_values,
                                                       const AliasingIFieldValues* values) {
  auto cmp = work_values->load_value_map.key_comp();
  auto work_it = work_values->load_value_map.begin(), work_end = work_values->load_value_map.end();
  auto store_it = values->store_ref_set.begin(), store_end = values->store_ref_set.end();
  auto load_it = values->load_value_map.begin(), load_end = values->load_value_map.end();
  while (store_it != store_end || load_it != load_end) {
    uint16_t ref;
    if (store_it != store_end && (load_it == load_end || *store_it < load_it->first)) {
      ref = *store_it;
      ++store_it;
    } else {
      ref = load_it->first;
      ++load_it;
      DCHECK(store_it == store_end || cmp(ref, *store_it));
    }
    while (work_it != work_end && cmp(work_it->first, ref)) {
      ++work_it;
    }
    if (work_it != work_end && !cmp(ref, work_it->first)) {
      // Already there.
      ++work_it;
    } else {
      // Store the key with arbitrary value, the correct value will be set later.
      work_values->load_value_map.PutHint(work_it, ref, 0u);
    }
  }
}

void LocalValueNumbering::MergeEscapedRefs(const ValueNameSet::value_type& entry,
                                           ValueNameSet::iterator hint) {
  // See if the ref is either escaped or non-aliasing in each predecessor.
  bool is_escaped = true;
  for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
    if (lvn->non_aliasing_refs_.count(entry) == 0u &&
        lvn->escaped_refs_.count(entry) == 0u) {
      is_escaped = false;
      break;
    }
  }
  if (is_escaped) {
    escaped_refs_.emplace_hint(hint, entry);
  }
}

void LocalValueNumbering::MergeEscapedIFieldTypeClobberSets(
    const EscapedIFieldClobberSet::value_type& entry, EscapedIFieldClobberSet::iterator hint) {
  // Insert only type-clobber entries (field_id == kNoValue) of escaped refs.
  if (entry.field_id == kNoValue && escaped_refs_.count(entry.base) != 0u) {
    escaped_ifield_clobber_set_.emplace_hint(hint, entry);
  }
}

void LocalValueNumbering::MergeEscapedIFieldClobberSets(
    const EscapedIFieldClobberSet::value_type& entry, EscapedIFieldClobberSet::iterator hint) {
  // Insert only those entries of escaped refs that are not overridden by a type clobber.
  if (!(hint == escaped_ifield_clobber_set_.end() &&
        hint->base == entry.base && hint->type == entry.type) &&
      escaped_refs_.count(entry.base) != 0u) {
    escaped_ifield_clobber_set_.emplace_hint(hint, entry);
  }
}

void LocalValueNumbering::MergeEscapedArrayClobberSets(
    const EscapedArrayClobberSet::value_type& entry, EscapedArrayClobberSet::iterator hint) {
  if (escaped_refs_.count(entry.base) != 0u) {
    escaped_array_clobber_set_.emplace_hint(hint, entry);
  }
}

void LocalValueNumbering::MergeNullChecked(const ValueNameSet::value_type& entry,
                                           ValueNameSet::iterator hint) {
  // Merge null_checked_ for this ref.
  merge_names_.clear();
  merge_names_.resize(gvn_->merge_lvns_.size(), entry);
  if (gvn_->NullCheckedInAllPredecessors(merge_names_)) {
    null_checked_.insert(hint, entry);
  }
}

void LocalValueNumbering::MergeSFieldValues(const SFieldToValueMap::value_type& entry,
                                            SFieldToValueMap::iterator hint) {
  uint16_t field_id = entry.first;
  merge_names_.clear();
  uint16_t value_name = kNoValue;
  bool same_values = true;
  for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
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
      if (gvn_->NullCheckedInAllPredecessors(merge_names_)) {
        null_checked_.insert(value_name);
      }
    }
  }
  sfield_value_map_.PutHint(hint, field_id, value_name);
}

void LocalValueNumbering::MergeNonAliasingIFieldValues(const IFieldLocToValueMap::value_type& entry,
                                                       IFieldLocToValueMap::iterator hint) {
  uint16_t field_loc = entry.first;
  merge_names_.clear();
  uint16_t value_name = kNoValue;
  bool same_values = true;
  for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
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
      if (gvn_->NullCheckedInAllPredecessors(merge_names_)) {
        null_checked_.insert(value_name);
      }
    }
  }
  non_aliasing_ifield_value_map_.PutHint(hint, field_loc, value_name);
}

void LocalValueNumbering::MergeAliasingIFieldValues(
    const AliasingIFieldValuesMap::value_type& entry, AliasingIFieldValuesMap::iterator hint) {
  uint16_t field_id = entry.first;
  uint16_t type = gvn_->GetFieldType(field_id);

  // Find the first values.
  const AliasingIFieldValues* cmp_values = nullptr;
  for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
    auto it = lvn->aliasing_ifield_value_map_.find(field_id);
    if (it != lvn->aliasing_ifield_value_map_.end()) {
      cmp_values = &it->second;
      break;
    }
  }
  DCHECK(cmp_values != nullptr);  // There must be at least one non-null values.

  // Check if we have identical memory versions, i.e. the global memory version, unresolved
  // field version and the values' memory_version_before_stores, last_stored_value
  // and store_ref_set are identical.
  bool same_version = global_memory_version_ != merge_new_memory_version_ &&
      unresolved_ifield_version_[type] != merge_new_memory_version_;
  if (same_version) {
    for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
      auto it = lvn->aliasing_ifield_value_map_.find(field_id);
      if (it == lvn->aliasing_ifield_value_map_.end()) {
        if (cmp_values->memory_version_before_stores != kNoValue) {
          same_version = false;
          break;
        }
      } else if (cmp_values->last_stored_value != it->second.last_stored_value ||
          cmp_values->memory_version_before_stores != it->second.memory_version_before_stores ||
          cmp_values->store_ref_set != it->second.store_ref_set) {
        same_version = false;
        break;
      }
    }
  }

  aliasing_ifield_value_map_.PutHint(hint, field_id, AliasingIFieldValues(gvn_->allocator_));
  DCHECK(hint != aliasing_ifield_value_map_.begin());
  AliasingIFieldValuesMap::iterator it = hint;
  --it;
  DCHECK_EQ(it->first, field_id);
  AliasingIFieldValues* my_values = &it->second;
  if (same_version) {
    my_values->memory_version_before_stores = cmp_values->memory_version_before_stores;
    my_values->last_stored_value = cmp_values->last_stored_value;
    my_values->store_ref_set = cmp_values->store_ref_set;
    // Merge load_value_maps and last_load_memory_version.
    for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
      auto it = lvn->aliasing_ifield_value_map_.find(field_id);
      if (it != lvn->aliasing_ifield_value_map_.end()) {
        if (it->second.last_load_memory_version != kNoValue) {
          DCHECK(my_values->last_load_memory_version == kNoValue ||
                 my_values->last_load_memory_version == it->second.last_load_memory_version);
          my_values->last_load_memory_version = it->second.last_load_memory_version;
        }
        InPlaceMapUnion(&my_values->load_value_map, it->second.load_value_map);
      }
    }
  } else {
    my_values->memory_version_before_stores = my_values->last_load_memory_version =
        gvn_->LookupValue(kMergeBlockAliasingIFieldVersionBumpOp, field_id, id_, kNoValue);
    // Calculate the union of all store and load sets.
    for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
      auto it = lvn->aliasing_ifield_value_map_.find(field_id);
      if (it != lvn->aliasing_ifield_value_map_.end()) {
        MergeAliasingIFieldValueRefs(my_values, &it->second);
      }
    }
    // Calculate merged values for the union.
    for (auto& entry : my_values->load_value_map) {
      uint16_t base = entry.first;
      bool same_values = true;
      uint16_t value_name = kNoValue;
      merge_names_.clear();
      for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
        auto it = lvn->aliasing_ifield_value_map_.find(field_id);
        if (it == lvn->aliasing_ifield_value_map_.end()) {
          uint16_t start_version =
              gvn_->LookupValue(kAliasingIFieldStartVersionOp, lvn->global_memory_version_,
                                lvn->unresolved_ifield_version_[type], field_id);
          value_name = gvn_->LookupValue(kAliasingIFieldOp, base, field_id, start_version);
        } else if (it->second.store_ref_set.count(base) != 0u) {
          value_name = it->second.last_stored_value;
        } else {
          auto load_it = it->second.load_value_map.find(base);
          if (load_it != it->second.load_value_map.end()) {
            value_name = load_it->second;
          } else {
            value_name = gvn_->LookupValue(kAliasingIFieldOp, base, field_id,
                                           it->second.last_load_memory_version);
          }
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
          // NOTE: In addition to field_id and id_ which don't change on an LVN recalculation
          // during GVN, we also add base which can actually change on recalculation, so the
          // value_name below may change. This could lead to an infinite loop if the base value
          // name always changed when the field value name changes. However, given that we assign
          // unique value names for other merges, such as Phis, such a dependency is not possible
          // in a well-formed SSA graph.
          value_name = gvn_->LookupValue(kMergeBlockMergeAliasingIFieldOp, field_id, id_, base);
          merge_map_.PutHint(lb, merge_names_, value_name);
          if (gvn_->NullCheckedInAllPredecessors(merge_names_)) {
            null_checked_.insert(value_name);
          }
        }
      }
      entry.second = value_name;
    }
  }
}

void LocalValueNumbering::Merge() {
  DCHECK_GE(gvn_->merge_lvns_.size(), 2u);

  MergeMemoryVersions();

  // We won't do anything complicated for range checks, just calculate the intersection.
  IntersectSets<RangeCheckSet, &LocalValueNumbering::range_checked_>();

  // Intersect the non-aliasing refs and merge escaped refs and clobber sets.
  IntersectSets<ValueNameSet, &LocalValueNumbering::non_aliasing_refs_>();
  MergeSets<ValueNameSet, &LocalValueNumbering::escaped_refs_,
            &LocalValueNumbering::MergeEscapedRefs>();
  if (!escaped_refs_.empty()) {
    MergeSets<EscapedIFieldClobberSet, &LocalValueNumbering::escaped_ifield_clobber_set_,
              &LocalValueNumbering::MergeEscapedIFieldTypeClobberSets>();
    MergeSets<EscapedIFieldClobberSet, &LocalValueNumbering::escaped_ifield_clobber_set_,
              &LocalValueNumbering::MergeEscapedIFieldClobberSets>();
    MergeSets<EscapedArrayClobberSet, &LocalValueNumbering::escaped_array_clobber_set_,
              &LocalValueNumbering::MergeEscapedArrayClobberSets>();
  }

  MergeSets<ValueNameSet, &LocalValueNumbering::null_checked_,
            &LocalValueNumbering::MergeNullChecked>();  // May later insert more.
  MergeSets<SFieldToValueMap, &LocalValueNumbering::sfield_value_map_,
            &LocalValueNumbering::MergeSFieldValues>();
  MergeSets<IFieldLocToValueMap, &LocalValueNumbering::non_aliasing_ifield_value_map_,
            &LocalValueNumbering::MergeNonAliasingIFieldValues>();
  MergeSets<AliasingIFieldValuesMap, &LocalValueNumbering::aliasing_ifield_value_map_,
            &LocalValueNumbering::MergeAliasingIFieldValues>();

  // Quick hack relying on the other Merge() that I want to rewrite!
  for (const LocalValueNumbering* other : gvn_->merge_lvns_) {
    if (other == gvn_->merge_lvns_[0]) {
      non_aliasing_array_version_map_ = other->non_aliasing_array_version_map_;
    } else {
      Merge(*other);
    }
  }
}

void LocalValueNumbering::Merge(const LocalValueNumbering& other) {
  // Merge local maps.
  MergeLocalMap(&non_aliasing_array_version_map_, other.non_aliasing_array_version_map_,
                kMergeBlockNonAliasingArrayVersionBumpOp);
}

uint16_t LocalValueNumbering::MarkNonAliasingNonNull(MIR* mir) {
  uint16_t res = gvn_->GetOperandValue(mir->ssa_rep->defs[0]);
  gvn_->SetOperandValue(mir->ssa_rep->defs[0], res);
  DCHECK(null_checked_.find(res) == null_checked_.end());
  null_checked_.insert(res);
  non_aliasing_refs_.insert(res);
  return res;
}

void LocalValueNumbering::Finish() {
  for (auto& entry : aliasing_ifield_value_map_) {
    UpdateAliasingIFieldMemoryVersion(entry.first, gvn_->GetFieldType(entry.first), &entry.second);
  }
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

uint16_t LocalValueNumbering::HandlePhi(MIR* mir) {
  if (gvn_->merge_lvns_.empty()) {
    // Running LVN without a full GVN?
    return kNoValue;
  }
  int16_t num_uses = mir->ssa_rep->num_uses;
  int32_t* uses = mir->ssa_rep->uses;
  // Try to find out if this is merging wide regs.
  if (mir->ssa_rep->defs[0] != 0 &&
      gvn_->sreg_wide_value_map_.find(mir->ssa_rep->defs[0] - 1) !=
          gvn_->sreg_wide_value_map_.end()) {
    // This is the high part of a wide reg. Ignore the Phi.
    return kNoValue;
  }
  bool wide = false;
  for (int16_t i = 0; i != num_uses; ++i) {
    if (gvn_->sreg_wide_value_map_.count(uses[i]) != 0u) {
      wide = true;
      break;
    }
  }
  // Iterate over *merge_lvns_ and skip incoming sregs for BBs without associated LVN.
  uint16_t value_name = kNoValue;
  merge_names_.clear();
  BasicBlockId* incoming = mir->meta.phi_incoming;
  int16_t pos = 0;
  bool same_values = true;
  for (const LocalValueNumbering* lvn : gvn_->merge_lvns_) {
    DCHECK_LT(pos, mir->ssa_rep->num_uses);
    while (incoming[pos] != lvn->Id()) {
      ++pos;
      DCHECK_LT(pos, mir->ssa_rep->num_uses);
    }
    int s_reg = uses[pos];
    ++pos;
    value_name = wide ? gvn_->GetOperandValueWide(s_reg) : gvn_->GetOperandValue(s_reg);

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
      value_name = gvn_->LookupValue(kNoValue, mir->ssa_rep->defs[0], kNoValue, kNoValue);
      merge_map_.PutHint(lb, merge_names_, value_name);
      if (!wide && gvn_->NullCheckedInAllPredecessors(merge_names_)) {
        null_checked_.insert(value_name);
      }
    }
  }
  if (wide) {
    gvn_->SetOperandValueWide(mir->ssa_rep->defs[0], value_name);
  } else {
    gvn_->SetOperandValue(mir->ssa_rep->defs[0], value_name);
  }
  return value_name;
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

LocalValueNumbering::AliasingIFieldValues* LocalValueNumbering::GetAliasingIFieldValues(
    uint16_t field_id) {
  auto lb = aliasing_ifield_value_map_.lower_bound(field_id);
  if (lb == aliasing_ifield_value_map_.end() || lb->first != field_id) {
    aliasing_ifield_value_map_.PutHint(lb, field_id, AliasingIFieldValues(gvn_->allocator_));
    // The new entry was inserted before lb.
    DCHECK(lb != aliasing_ifield_value_map_.begin());
    --lb;
    DCHECK_EQ(lb->first, field_id);
  }
  return &lb->second;
}

void LocalValueNumbering::UpdateAliasingIFieldMemoryVersion(uint16_t field_id, uint16_t type,
                                                            AliasingIFieldValues* values) {
  if (values->last_load_memory_version == kNoValue) {
    // Get the start version that accounts for aliasing with unresolved fields of the same
    // type and make it unique for the field by including the field_id.
    uint16_t memory_version = values->memory_version_before_stores;
    if (memory_version == kNoValue) {
      memory_version = gvn_->LookupValue(kAliasingIFieldStartVersionOp, global_memory_version_,
                                         unresolved_ifield_version_[type], field_id);
    }
    if (!values->store_ref_set.empty()) {
      uint16_t ref_set_id = gvn_->GetRefSetId(values->store_ref_set);
      memory_version = gvn_->LookupValue(kAliasingIFieldBumpVersionOp, memory_version,
                                         ref_set_id, values->last_stored_value);
    }
    values->last_load_memory_version = memory_version;
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
      // Get the local AliasingIFieldValues.
      AliasingIFieldValues* values = GetAliasingIFieldValues(field_id);
      if (values->store_ref_set.count(base) != 0u) {
        res = values->last_stored_value;
      } else {
        UpdateAliasingIFieldMemoryVersion(field_id, type, values);
        auto lb = values->load_value_map.lower_bound(base);
        if (lb != values->load_value_map.end() && lb->first == base) {
          res = lb->second;
        } else {
          res = gvn_->LookupValue(kAliasingIFieldOp, base, field_id,
                                  values->last_load_memory_version);
          values->load_value_map.PutHint(lb, base, res);
        }
      }
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

    // Aliasing fields of the same type may have been overwritten.
    auto it = aliasing_ifield_value_map_.begin(), end = aliasing_ifield_value_map_.end();
    while (it != end) {
      if (gvn_->GetFieldType(it->first) != type) {
        ++it;
      } else {
        it = aliasing_ifield_value_map_.erase(it);
      }
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
      AliasingIFieldValues* values = GetAliasingIFieldValues(field_id);
      auto load_values_it = values->load_value_map.find(base);
      if (load_values_it != values->load_value_map.end() && load_values_it->second == value) {
        // This IPUT can be eliminated, it stores the same value that's already in the field.
        // TODO: Eliminate the IPUT.
        return;
      }
      if (value == values->last_stored_value) {
        auto store_ref_lb = values->store_ref_set.lower_bound(base);
        if (store_ref_lb != values->store_ref_set.end() && *store_ref_lb == base) {
          // This IPUT can be eliminated, it stores the same value that's already in the field.
          // TODO: Eliminate the IPUT.
          return;
        }
        values->store_ref_set.emplace_hint(store_ref_lb, base);
      } else {
        UpdateAliasingIFieldMemoryVersion(field_id, type, values);
        values->memory_version_before_stores = values->last_load_memory_version;
        values->last_stored_value = value;
        values->store_ref_set.clear();
        values->store_ref_set.insert(base);
      }
      // Clear the last load memory version and remove all potentially overwritten values.
      values->last_load_memory_version = kNoValue;
      auto it = values->load_value_map.begin(), end = values->load_value_map.end();
      while (it != end) {
        if (it->second == value) {
          ++it;
        } else {
          it = values->load_value_map.erase(it);
        }
      }

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
        aliasing_ifield_value_map_.clear();
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
      res = HandlePhi(mir);
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

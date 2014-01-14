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

#include "zygote_space.h"

#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/space_bitmap-inl.h"
#include "gc/heap.h"
#include "thread-inl.h"
#include "utils.h"

namespace art {
namespace gc {
namespace space {

class CountObjectsAllocated {
 public:
  explicit CountObjectsAllocated()
      : objects_allocated_(0) {}

  void operator()(mirror::Object* obj) const {
    ++*const_cast<size_t*>(&objects_allocated_);
  }

  size_t GetObjectsAllocated() const {
    return objects_allocated_;
  }

 private:
  size_t objects_allocated_;
};

ZygoteSpace* ZygoteSpace::Create(const std::string& name, MemMap* mem_map,
                                 accounting::SpaceBitmap* live_bitmap,
                                 accounting::SpaceBitmap* mark_bitmap) {
  DCHECK(live_bitmap != nullptr);
  DCHECK(mark_bitmap != nullptr);
  CountObjectsAllocated visitor;
  ReaderMutexLock mu(Thread::Current(), *Locks::heap_bitmap_lock_);
  live_bitmap->VisitMarkedRange(reinterpret_cast<uintptr_t>(mem_map->Begin()),
                                reinterpret_cast<uintptr_t>(mem_map->End()), visitor);
  ZygoteSpace* zygote_space = new ZygoteSpace(name, mem_map, visitor.GetObjectsAllocated());
  zygote_space->live_bitmap_.reset(live_bitmap);
  zygote_space->mark_bitmap_.reset(mark_bitmap);
  return zygote_space;
}

ZygoteSpace::ZygoteSpace(const std::string& name, MemMap* mem_map, size_t objects_allocated)
    : MallocSpace(name, mem_map, mem_map->Begin(), mem_map->End(), mem_map->End(),
                  mem_map->Size(), false),
      objects_allocated_(objects_allocated) {
  SetGcRetentionPolicy(kGcRetentionPolicyFullCollect);
}

void ZygoteSpace::Dump(std::ostream& os) const {
  os << GetType()
      << " begin=" << reinterpret_cast<void*>(Begin())
      << ",end=" << reinterpret_cast<void*>(End())
      << ",size=" << PrettySize(Size())
      << ",name=\"" << GetName() << "\"]";
}

void ZygoteSpace::SweepCallback(size_t num_ptrs, mirror::Object** ptrs, void* arg) {
  SweepCallbackContext* context = static_cast<SweepCallbackContext*>(arg);
  DCHECK(context->space->IsZygoteSpace());
  ZygoteSpace* zygote_space = context->space->AsZygoteSpace();
  Locks::heap_bitmap_lock_->AssertExclusiveHeld(context->self);
  accounting::CardTable* card_table = context->heap->GetCardTable();
  // If the bitmaps aren't swapped we need to clear the bits since the GC isn't going to re-swap
  // the bitmaps as an optimization.
  if (!context->swap_bitmaps) {
    accounting::SpaceBitmap* bitmap = zygote_space->GetLiveBitmap();
    for (size_t i = 0; i < num_ptrs; ++i) {
      bitmap->Clear(ptrs[i]);
    }
  }
  // We don't free any actual memory to avoid dirtying the shared zygote pages.
  for (size_t i = 0; i < num_ptrs; ++i) {
    // Need to mark the card since this will update the mod-union table next GC cycle.
    card_table->MarkCard(ptrs[i]);
  }
}

}  // namespace space
}  // namespace gc
}  // namespace art

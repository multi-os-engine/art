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

#include "reference_processor.h"

#include "mirror/object-inl.h"
#include "mirror/reference-inl.h"
#include "reflection.h"
#include "ScopedLocalRef.h"
#include "well_known_classes.h"

namespace art {
namespace gc {

ReferenceProcessor::ReferenceProcessor()
    : slow_path_enabled_(false),
      lock_("reference processor lock"),
      condition_("reference processor condition", lock_) {
}

void ReferenceProcessor::EnableSlowPath() {
  slow_path_enabled_ = true;
}

void ReferenceProcessor::DisableSlowPath() {
  Thread* self = Thread::Current();
  MutexLock mu(self, lock_);
  slow_path_enabled_ = false;
  condition_.Broadcast(self);
}

mirror::Object* ReferenceProcessor::GetReferent(Thread* self, mirror::Reference* reference) {
  if (LIKELY(!slow_path_enabled_)) {
    return reference->GetReferent();
  }
  MutexLock mu(self, lock_);
  while (slow_path_enabled_) {
    condition_.Wait(self);
  }
  return reference->GetReferent();
}

struct SoftReferenceArgs {
  IsMarkedCallback* const is_marked_callback_;
  MarkObjectCallback* const mark_callback_;
  void* const arg_;
};

mirror::Object* ReferenceProcessor::PreserveSoftReferenceCallback(mirror::Object* obj, void* arg) {
  SoftReferenceArgs* const args = reinterpret_cast<SoftReferenceArgs*>(arg);
  // TODO: Not preserve all soft references.
  return args->mark_callback_(obj, args->arg_);
}

// Process reference class instances and schedule finalizations.
void ReferenceProcessor::ProcessReferences(bool concurrent, TimingLogger* timings,
                                           bool clear_soft_references,
                                           IsMarkedCallback* is_marked_callback,
                                           MarkObjectCallback* mark_object_callback,
                                           ProcessMarkStackCallback* process_mark_stack_callback,
                                           void* arg) {
  if (concurrent) {
    CHECK(slow_path_enabled_) << "Slow path must be enabled for concurrent reference processing";
    timings->StartSplit("ProcessReferences");
  } else {
    timings->StartSplit("(Paused)ProcessReferences");
  }
  // Unless required to clear soft references with white references, preserve some white referents.
  if (!clear_soft_references) {
    // Don't clear for sticky GC.
    SoftReferenceArgs soft_reference_args = {is_marked_callback, mark_object_callback, arg};
    // References with a marked referent are removed from the list.
    soft_reference_queue_.PreserveSomeSoftReferences(&PreserveSoftReferenceCallback,
                                                     &soft_reference_args);
    process_mark_stack_callback(arg);
  }
  // Clear all remaining soft and weak references with white referents.
  soft_reference_queue_.ClearWhiteReferences(cleared_references_, is_marked_callback, arg);
  weak_reference_queue_.ClearWhiteReferences(cleared_references_, is_marked_callback, arg);
  timings->EndSplit();
  // Preserve all white objects with finalize methods and schedule them for finalization.
  timings->StartSplit("(Paused)EnqueueFinalizerReferences");
  finalizer_reference_queue_.EnqueueFinalizerReferences(cleared_references_, is_marked_callback,
                                                        mark_object_callback, arg);
  process_mark_stack_callback(arg);
  timings->EndSplit();
  timings->StartSplit("(Paused)ProcessReferences");
  // Clear all f-reachable soft and weak references with white referents.
  soft_reference_queue_.ClearWhiteReferences(cleared_references_, is_marked_callback, arg);
  weak_reference_queue_.ClearWhiteReferences(cleared_references_, is_marked_callback, arg);
  // Clear all phantom references with white referents.
  phantom_reference_queue_.ClearWhiteReferences(cleared_references_, is_marked_callback, arg);
  // At this point all reference queues other than the cleared references should be empty.
  DCHECK(soft_reference_queue_.IsEmpty());
  DCHECK(weak_reference_queue_.IsEmpty());
  DCHECK(finalizer_reference_queue_.IsEmpty());
  DCHECK(phantom_reference_queue_.IsEmpty());
  timings->EndSplit();
  if (concurrent) {
    // Done processing, disable the slow path and broadcast to the waiters.
    DisableSlowPath();
  }
}

// Process the "referent" field in a java.lang.ref.Reference.  If the referent has not yet been
// marked, put it on the appropriate list in the heap for later processing.
void ReferenceProcessor::DelayReferenceReferent(mirror::Class* klass, mirror::Reference* ref,
                                                IsMarkedCallback is_marked_callback, void* arg) {
  // klass can be the class of the old object if the visitor already updated the class of ref.
  DCHECK(klass->IsReferenceClass());
  mirror::Object* referent = ref->GetReferent();
  if (referent != nullptr) {
    mirror::Object* forward_address = is_marked_callback(referent, arg);
    // Null means that the object is not currently marked.
    if (forward_address == nullptr) {
      Thread* self = Thread::Current();
      // TODO: Remove these locks, and use atomic stacks for storing references?
      // We need to check that the references haven't already been enqueued since we can end up
      // scanning the same reference multiple times due to dirty cards.
      if (klass->IsSoftReferenceClass()) {
        soft_reference_queue_.AtomicEnqueueIfNotEnqueued(self, ref);
      } else if (klass->IsWeakReferenceClass()) {
        weak_reference_queue_.AtomicEnqueueIfNotEnqueued(self, ref);
      } else if (klass->IsFinalizerReferenceClass()) {
        finalizer_reference_queue_.AtomicEnqueueIfNotEnqueued(self, ref);
      } else if (klass->IsPhantomReferenceClass()) {
        phantom_reference_queue_.AtomicEnqueueIfNotEnqueued(self, ref);
      } else {
        LOG(FATAL) << "Invalid reference type " << PrettyClass(klass) << " " << std::hex
                   << klass->GetAccessFlags();
      }
    } else if (referent != forward_address) {
      // Referent is already marked and we need to update it.
      ref->SetReferent<false>(forward_address);
    }
  }
}

void ReferenceProcessor::EnqueueClearedReferences() {
  Thread* self = Thread::Current();
  Locks::mutator_lock_->AssertNotHeld(self);
  if (!cleared_references_.IsEmpty()) {
    // When a runtime isn't started there are no reference queues to care about so ignore.
    if (LIKELY(Runtime::Current()->IsStarted())) {
      ScopedObjectAccess soa(self);
      ScopedLocalRef<jobject> arg(self->GetJniEnv(),
                                  soa.AddLocalReference<jobject>(cleared_references_.GetList()));
      jvalue args[1];
      args[0].l = arg.get();
      InvokeWithJValues(soa, nullptr, WellKnownClasses::java_lang_ref_ReferenceQueue_add, args);
    }
    cleared_references_.Clear();
  }
}

}  // namespace gc
}  // namespace art


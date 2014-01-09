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

#ifndef ART_RUNTIME_CATCH_BLOCK_STACK_VISITOR_H_
#define ART_RUNTIME_CATCH_BLOCK_STACK_VISITOR_H_

#include "mirror/throwable.h"
#include "thread.h"

namespace art {
class CatchFinder;
class ThrowLocation;

// Finds catch handler or prepares deoptimization.
class CatchBlockStackVisitor : public StackVisitor {
 public:
  CatchBlockStackVisitor(Thread* self, Context* context, const ThrowLocation& throw_location,
                         mirror::Throwable* exception, bool is_deoptimization,
                         CatchFinder& catch_finder)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(self, context),
        self_(self), exception_(exception), is_deoptimization_(is_deoptimization),
        to_find_(is_deoptimization ? nullptr : exception->GetClass()), throw_location_(throw_location),
        catch_finder_(catch_finder),
        native_method_count_(0),
        prev_shadow_frame_(nullptr) {
  }

  bool VisitFrame() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  bool HandleTryItems(mirror::ArtMethod* method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool HandleDeoptimization(mirror::ArtMethod* m) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Thread* const self_;
  mirror::Throwable* const exception_;
  const bool is_deoptimization_;
  // The type of the exception catch block to find.
  mirror::Class* const to_find_;
  // Location of the throw.
  const ThrowLocation& throw_location_;
  CatchFinder& catch_finder_;
  // Number of native methods passed in crawl (equates to number of SIRTs to pop)
  uint32_t native_method_count_;
  ShadowFrame* prev_shadow_frame_;
};

}  // namespace art
#endif  // ART_RUNTIME_CATCH_BLOCK_STACK_VISITOR_H_

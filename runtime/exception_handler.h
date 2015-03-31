/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_EXCEPTION_HANDLER_H_
#define ART_RUNTIME_EXCEPTION_HANDLER_H_

#include "base/logging.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "stack.h"  // StackReference

namespace art {

namespace mirror {
class ArtMethod;
class Throwable;
}  // namespace mirror
class Context;
class Thread;
class QuickExceptionHandler;

// Finds catch handler or prepares for deoptimization.
class CatchBlockStackVisitor FINAL : public StackVisitor {
 public:
  CatchBlockStackVisitor(Thread* self, Context* context, Handle<mirror::Throwable>* exception,
                         QuickExceptionHandler* exception_handler)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_)
      : StackVisitor(self, context), self_(self), exception_(exception),
        exception_handler_(exception_handler) {
  }

  bool VisitFrame() OVERRIDE SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

 private:
  bool LookForCatch(Handle<mirror::ArtMethod> method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool LookForCatchOptimizing(Handle<mirror::ArtMethod> method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  Thread* const self_;
  // The exception we're looking for the catch block of.
  Handle<mirror::Throwable>* exception_;
  // The quick exception handler we're visiting for.
  QuickExceptionHandler* const exception_handler_;

  DISALLOW_COPY_AND_ASSIGN(CatchBlockStackVisitor);
};

}  // namespace art
#endif  // ART_RUNTIME_EXCEPTION_HANDLER_H_

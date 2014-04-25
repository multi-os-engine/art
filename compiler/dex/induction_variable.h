/*
 * Copyright (C) 2014 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_COMPILER_DEX_INDUCTION_VARIABLE_H_
#define ART_COMPILER_DEX_INDUCTION_VARIABLE_H_


namespace art {

  // An induction variable is updated in the form iv = multiplier * iv2 + increment
  // Where iv2 may or not be iv, making it a basic IV or not
  class InductionVariable {
    protected:
      int target_vr_;

      int multiplier_;
      int increment_;

      // Can be nullptr, thus basic
      InductionVariable* dependent_;

    public:
      InductionVariable(int tvr, int multiplier, int increment, InductionVariable* d = nullptr)
        :target_vr_(tvr), multiplier_(multiplier), increment_(increment), dependent_(d) {
      }

      bool IsBasic() const {
        return dependent_ == nullptr;
      }

      bool GetIncrement() const {
        return increment_;
      }

      bool GetMultiplier() const {
        return multiplier_;
      }

      bool IsLinear() const {
        return GetMultiplier() == 1;
      }

      int GetVR() const {
        return target_vr_;
      }

      InductionVariable* GetDependent() const {
        return dependent_;
      }

      bool IsBasicAndIncrementOf1() const {
        return IsBasic() == true && GetIncrement() == 1;
      }

      static void* operator new(size_t size, ArenaAllocator* arena) {
        return arena->Alloc(sizeof(InductionVariable), kArenaAllocMisc);
      }
      static void operator delete(void* p) {}  // Nop.
  };

}  // namespace art

#endif  // ART_COMPILER_DEX_INDUCTION_VARIABLE_H_

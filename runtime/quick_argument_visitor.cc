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

#include "quick_argument_visitor.h"

namespace art {

uint8_t* QuickArgumentVisitor::GetParamAddress() const {
  if (!kQuickSoftFloatAbi) {
    Primitive::Type type = GetParamPrimitiveType();
    if (UNLIKELY((type == Primitive::kPrimDouble) || (type == Primitive::kPrimFloat))) {
      if (type == Primitive::kPrimDouble && kQuickDoubleRegAlignedFloatBackFilled) {
        if (fpr_double_index_ + 2 < kNumQuickFprArgs + 1) {
          return fpr_args_ + (fpr_double_index_ * GetBytesPerFprSpillLocation(kRuntimeISA));
        }
      } else if (fpr_index_ + 1 < kNumQuickFprArgs + 1) {
        return fpr_args_ + (fpr_index_ * GetBytesPerFprSpillLocation(kRuntimeISA));
      }
      return stack_args_ + (stack_index_ * kBytesStackArgLocation);
    }
  }
  if (gpr_index_ < kNumQuickGprArgs) {
    return gpr_args_ + GprIndexToGprOffset(gpr_index_);
  }
  return stack_args_ + (stack_index_ * kBytesStackArgLocation);
}

void QuickArgumentVisitor::VisitArguments() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // (a) 'stack_args_' should point to the first method's argument
  // (b) whatever the argument type it is, the 'stack_index_' should
  //     be moved forward along with every visiting.
  gpr_index_ = 0;
  fpr_index_ = 0;
  if (kQuickDoubleRegAlignedFloatBackFilled) {
    fpr_double_index_ = 0;
  }
  stack_index_ = 0;
  if (!is_static_) {  // Handle this.
    cur_type_ = Primitive::kPrimNot;
    is_split_long_or_double_ = false;
    Visit();
    stack_index_++;
    if (kNumQuickGprArgs > 0) {
      gpr_index_++;
    }
  }
  for (uint32_t shorty_index = 1; shorty_index < shorty_len_; ++shorty_index) {
    cur_type_ = Primitive::GetType(shorty_[shorty_index]);
    switch (cur_type_) {
      case Primitive::kPrimNot:
      case Primitive::kPrimBoolean:
      case Primitive::kPrimByte:
      case Primitive::kPrimChar:
      case Primitive::kPrimShort:
      case Primitive::kPrimInt:
        is_split_long_or_double_ = false;
        Visit();
        stack_index_++;
        if (gpr_index_ < kNumQuickGprArgs) {
          gpr_index_++;
        }
        break;
      case Primitive::kPrimFloat:
        is_split_long_or_double_ = false;
        Visit();
        stack_index_++;
        if (kQuickSoftFloatAbi) {
          if (gpr_index_ < kNumQuickGprArgs) {
            gpr_index_++;
          }
        } else {
          if (fpr_index_ + 1 < kNumQuickFprArgs + 1) {
            fpr_index_++;
            if (kQuickDoubleRegAlignedFloatBackFilled) {
              // Double should not overlap with float.
              // For example, if fpr_index_ = 3, fpr_double_index_ should be at least 4.
              fpr_double_index_ = std::max(fpr_double_index_, RoundUp(fpr_index_, 2));
              // Float should not overlap with double.
              if (fpr_index_ % 2 == 0) {
                fpr_index_ = std::max(fpr_double_index_, fpr_index_);
              }
            }
          }
        }
        break;
      case Primitive::kPrimDouble:
      case Primitive::kPrimLong:
        if (kQuickSoftFloatAbi || (cur_type_ == Primitive::kPrimLong)) {
          is_split_long_or_double_ = (GetBytesPerGprSpillLocation(kRuntimeISA) == 4) &&
              ((gpr_index_ + 1) == kNumQuickGprArgs);
          Visit();
          if (kBytesStackArgLocation == 4) {
            stack_index_+= 2;
          } else {
            CHECK_EQ(kBytesStackArgLocation, 8U);
            stack_index_++;
          }
          if (gpr_index_ < kNumQuickGprArgs) {
            gpr_index_++;
            if (GetBytesPerGprSpillLocation(kRuntimeISA) == 4) {
              if (gpr_index_ < kNumQuickGprArgs) {
                gpr_index_++;
              }
            }
          }
        } else {
          is_split_long_or_double_ = (GetBytesPerFprSpillLocation(kRuntimeISA) == 4) &&
              ((fpr_index_ + 1) == kNumQuickFprArgs) && !kQuickDoubleRegAlignedFloatBackFilled;
          Visit();
          if (kBytesStackArgLocation == 4) {
            stack_index_+= 2;
          } else {
            CHECK_EQ(kBytesStackArgLocation, 8U);
            stack_index_++;
          }
          if (kQuickDoubleRegAlignedFloatBackFilled) {
            if (fpr_double_index_ + 2 < kNumQuickFprArgs + 1) {
              fpr_double_index_ += 2;
              // Float should not overlap with double.
              if (fpr_index_ % 2 == 0) {
                fpr_index_ = std::max(fpr_double_index_, fpr_index_);
              }
            }
          } else if (fpr_index_ + 1 < kNumQuickFprArgs + 1) {
            fpr_index_++;
            if (GetBytesPerFprSpillLocation(kRuntimeISA) == 4) {
              if (fpr_index_ + 1 < kNumQuickFprArgs + 1) {
                fpr_index_++;
              }
            }
          }
        }
        break;
      default:
        LOG(FATAL) << "Unexpected type: " << cur_type_ << " in " << shorty_;
    }
  }
}


void BuildQuickArgumentVisitor::Visit() {
  jvalue val;
  Primitive::Type type = GetParamPrimitiveType();
  switch (type) {
    case Primitive::kPrimNot: {
      StackReference<mirror::Object>* stack_ref =
          reinterpret_cast<StackReference<mirror::Object>*>(GetParamAddress());
      val.l = soa_->AddLocalReference<jobject>(stack_ref->AsMirrorPtr());
      references_.push_back(std::make_pair(val.l, stack_ref));
      break;
    }
    case Primitive::kPrimLong:  // Fall-through.
    case Primitive::kPrimDouble:
      if (IsSplitLongOrDouble()) {
        val.j = ReadSplitLongParam();
      } else {
        val.j = *reinterpret_cast<jlong*>(GetParamAddress());
      }
      break;
    case Primitive::kPrimBoolean:  // Fall-through.
    case Primitive::kPrimByte:     // Fall-through.
    case Primitive::kPrimChar:     // Fall-through.
    case Primitive::kPrimShort:    // Fall-through.
    case Primitive::kPrimInt:      // Fall-through.
    case Primitive::kPrimFloat:
      val.i = *reinterpret_cast<jint*>(GetParamAddress());
      break;
    case Primitive::kPrimVoid:
      LOG(FATAL) << "UNREACHABLE";
      UNREACHABLE();
  }
  args_->push_back(val);
}

void BuildQuickArgumentVisitor::FixupReferences() {
  // Fixup any references which may have changed.
  for (const auto& pair : references_) {
    pair.second->Assign(soa_->Decode<mirror::Object*>(pair.first));
    soa_->Env()->DeleteLocalRef(pair.first);
  }
}

}  // namespace art

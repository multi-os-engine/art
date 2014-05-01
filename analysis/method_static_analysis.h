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

#ifndef ART_ANALYSIS_METHOD_STATIC_ANALYSIS_H_
#define ART_ANALYSIS_METHOD_STATIC_ANALYSIS_H_

#include "atomic.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "base/stl_util.h"
#include "dex_instruction-inl.h"
#include "mirror/art_method-inl.h"
#include "object_utils.h"
#include "static_analysis_info.h"
#include "static_analysis_pass.h"
#include "verifier/method_verifier-inl.h"

namespace art {
/**
 * @class StaticAnalysisMethodCumulativeLogisticsStats
 * @brief Holder for the cumulative stats for logistics.
 */
class StaticAnalysisMethodCumulativeLogisticsStats : public StaticAnalysisMethodCumulativeStats {
 public:
  StaticAnalysisMethodCumulativeLogisticsStats() {
    num_of_methods_.StoreRelaxed(0);
    num_of_native_methods_.StoreRelaxed(0);
    num_of_abstract_methods_.StoreRelaxed(0);
    num_of_analyzable_methods_.StoreRelaxed(0);
  }
  /** Cumulative count of methods. */
  Atomic<uint32_t> num_of_methods_;
  /** Cumulative count of native methods. */
  Atomic<uint32_t> num_of_native_methods_;
  /** Cumulative count of abstract methods. */
  Atomic<uint32_t> num_of_abstract_methods_;
  /**
   * @brief Cumulative count of analyzable methods.
   * "Analyzable" meaning the number of methods we can perform static analysis on.
   * Also is numOfAnalyzableMethods = numOfMethods - (numOfNative + numOfAbstractMethods). */
  Atomic<uint32_t> num_of_analyzable_methods_;
};

/**
 * @class MethodLogisticsAnalysis
 * @brief Perform the Method Logistics Analysis pass.
 * Looks at the type of method.
 * APK Level:
 *  Keeps a cumulative count for the following:
 *    # of Total Methods.
 *    # of Native Methods.
 *    # of Abstract Methods.
 *    # of Analyzable Methods.
 *
 * Method Level:
 *  Returns a static analysis method info bitmap to indicate the following:
 *    Nothing.
 */
class MethodLogisticsAnalysis : public StaticAnalysisPass {
 public:
  MethodLogisticsAnalysis() :
      StaticAnalysisPass("MethodLogisticsAnalysis"),
      static_analysis_method_cumulative_logistics_stats_(GetPassStatsInstance<StaticAnalysisMethodCumulativeLogisticsStats>()) {
  }

  /**
   * @fn PerformAnalysis(mirror::ArtMethod* method, const DexFile& dex_file)
   * @brief Performs the logistics analysis of the method to accumulate the number of
   *  1)Total Methods, 2) Native Methods, 3) Abstract Methods, and 4) Analyzable Methods.
   * @param method an ArtMethod pointer.
   * @param dex_file a dex file reference.
   * @param verifier a method verifier for our particular method.
   * @return A bit mask equaling 0x0.
   */
  uint32_t PerformAnalysis(StaticAnalysisMethodCumulativeStats* stats, mirror::ArtMethod* method, const DexFile* dex_file, verifier::MethodVerifier* verifier) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    UNUSED(dex_file);
    StaticAnalysisMethodCumulativeLogisticsStats* static_analysis_method_cumulative_logistics_stats = down_cast<StaticAnalysisMethodCumulativeLogisticsStats*>(stats);
    if (method != nullptr) {
      ++static_analysis_method_cumulative_logistics_stats->num_of_methods_;
      if (method->IsNative()) {
        ++static_analysis_method_cumulative_logistics_stats->num_of_native_methods_;
      } else if (method->IsAbstract()) {
        ++static_analysis_method_cumulative_logistics_stats->num_of_abstract_methods_;
      } else {
        ++static_analysis_method_cumulative_logistics_stats->num_of_analyzable_methods_;
      }
    } else if (verifier != nullptr) {
        ++static_analysis_method_cumulative_logistics_stats->num_of_methods_;
        uint32_t access_flags = verifier->GetAccessFlags();
        if ((access_flags & kAccNative) != 0) {
          ++static_analysis_method_cumulative_logistics_stats->num_of_native_methods_;
        } else if ((access_flags & kAccAbstract) != 0) {
          ++static_analysis_method_cumulative_logistics_stats->num_of_abstract_methods_;
        } else {
          ++static_analysis_method_cumulative_logistics_stats->num_of_analyzable_methods_;
        }
    }
    return kMethodNone;
  }

  /**
   * @fn DumpPassAnalysis(std::stringstream& string_stream) const
   * @brief Dumps stats for 1)Total Methods, 2) Native Methods, 3) Abstract Methods, and 4) Analyzable Methods
   *  for debugging and data collection purposes.
   * @param[out] string_stream a concatenated stringstream containing the stats for the pass
   *  that can later be used in a LOG statement.
   *  e.x. LOG(INFO) << my_string_stream.str().
   */
  void DumpPassAnalysis(std::stringstream& string_stream) const {
    string_stream << static_analysis_method_cumulative_logistics_stats_->num_of_methods_.LoadRelaxed() <<" methods total. "
        << static_analysis_method_cumulative_logistics_stats_->num_of_native_methods_.LoadRelaxed() << " native methods found. "
        << static_analysis_method_cumulative_logistics_stats_->num_of_abstract_methods_.LoadRelaxed() << " abstract methods found. "
        << static_analysis_method_cumulative_logistics_stats_->num_of_analyzable_methods_.LoadRelaxed() << " analyzable methods found. ";
  }

  StaticAnalysisMethodCumulativeStats* GetStats() const {
    return GetPassStatsInstance<StaticAnalysisMethodCumulativeLogisticsStats>();
  }

  StaticAnalysisMethodCumulativeLogisticsStats* static_analysis_method_cumulative_logistics_stats_;
};

/**
 * @class StaticAnalysisMethodCumulativeMiscLogisticsStats
 * @brief Holder for the cumulative stats for misc. logistics.
 */
class StaticAnalysisMethodCumulativeMiscLogisticsStats : public StaticAnalysisMethodCumulativeStats {
 public:
  StaticAnalysisMethodCumulativeMiscLogisticsStats() {
    num_of_methods_that_are_constructors_.StoreRelaxed(0);
  }
  /** Cumulative count of methods that constructors. */
  Atomic<uint32_t> num_of_methods_that_are_constructors_;
};

/**
 * @class MethodMiscLogisticsAnalysis
 * @brief Perform the Method Logistics Analysis pass.
 * Looks at miscellaneous properties of a method.
 * APK Level:
 *  Keeps a cumulative count for the following:
 *    Number of Methods that are Constructors.
 *
 * Method Level:
 *  Returns a static analysis method info bitmap to indicate the following:
 *    Nothing.
 */
class MethodMiscLogisticsAnalysis : public StaticAnalysisPass {
 public:
  MethodMiscLogisticsAnalysis() :
      StaticAnalysisPass("MethodMiscLogisticsAnalysis"),
      static_analysis_method_cumulative_misc_logistics_stats_(GetPassStatsInstance<StaticAnalysisMethodCumulativeMiscLogisticsStats>()) {
  }

  /**
   * @fn PerformAnalysis(mirror::ArtMethod* method, const DexFile& dex_file)
   * @brief Performs the misc logistics analysis of the method to accumulate the number of
   *  1) methods that constructors.
   * @param method an ArtMethod pointer.
   * @param dex_file a dex file reference.
   * @param verifier a method verifier for our particular method.
   * @return A bit mask equaling 0x0.
   */
  uint32_t PerformAnalysis(StaticAnalysisMethodCumulativeStats* stats, mirror::ArtMethod* method, const DexFile* dex_file, verifier::MethodVerifier* verifier) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    UNUSED(dex_file);
    StaticAnalysisMethodCumulativeMiscLogisticsStats* static_analysis_method_cumulative_misc_logistics_stats = down_cast<StaticAnalysisMethodCumulativeMiscLogisticsStats*>(stats);
    if (method != nullptr) {
      if (method->IsConstructor()) {
        ++static_analysis_method_cumulative_misc_logistics_stats->num_of_methods_that_are_constructors_;
      }
    } else if (verifier != nullptr) {
      if ((verifier->GetAccessFlags() & kAccConstructor) != 0) {
        ++static_analysis_method_cumulative_misc_logistics_stats->num_of_methods_that_are_constructors_;
      }
    }
    return kMethodNone;
  }

  /**
   * @fn DumpPassAnalysis(std::stringstream& string_stream) const
   * @brief Dumps stats for 1)methods that constructors.
   *  for debugging and data collection purposes.
   * @param[out] string_stream a concatenated stringstream containing the stats for the pass
   *  that can later be used in a LOG statement.
   * e.x. LOG(INFO) << my_string_stream.str().
   */
  void DumpPassAnalysis(std::stringstream& string_stream) const {
    string_stream << static_analysis_method_cumulative_misc_logistics_stats_->num_of_methods_that_are_constructors_.LoadRelaxed() << " constructor methods. ";
  }

  StaticAnalysisMethodCumulativeStats* GetStats() const {
    return GetPassStatsInstance<StaticAnalysisMethodCumulativeMiscLogisticsStats>();
  }

  StaticAnalysisMethodCumulativeMiscLogisticsStats* static_analysis_method_cumulative_misc_logistics_stats_;
};

/**
 * @class StaticAnalysisMethodCumulativeSizeStats
 * @brief Holder for the cumulative stats for size analysis.
 */
class StaticAnalysisMethodCumulativeSizeStats : public StaticAnalysisMethodCumulativeStats {
 public:
  StaticAnalysisMethodCumulativeSizeStats() {
    size_of_all_methods_.StoreRelaxed(0);
    num_of_sub_tiny_methods_.StoreRelaxed(0);
    num_of_tiny_methods_.StoreRelaxed(0);
    num_of_sub_small_methods_.StoreRelaxed(0);
    num_of_small_methods_.StoreRelaxed(0);
    num_of_medium_methods_.StoreRelaxed(0);
    num_of_large_methods_.StoreRelaxed(0);
    num_of_too_large_methods_.StoreRelaxed(0);
  }
  /** Cumulative count of each instruction in 16-bit code units. */
  Atomic<uint32_t> size_of_all_methods_;
  /** Cumulative count of sub tiny methods. */
  Atomic<uint32_t> num_of_sub_tiny_methods_;
  /** Cumulative count of tiny methods. */
  Atomic<uint32_t> num_of_tiny_methods_;
  /** Cumulative count of sub small methods. */
  Atomic<uint32_t> num_of_sub_small_methods_;
  /** Cumulative count of small methods. */
  Atomic<uint32_t> num_of_small_methods_;
  /** Cumulative count of medium methods. */
  Atomic<uint32_t> num_of_medium_methods_;
  /** Cumulative count of large methods. */
  Atomic<uint32_t> num_of_large_methods_;
  /** Cumulative count of too large methods. */
  Atomic<uint32_t> num_of_too_large_methods_;
};

/**
 * @class MethodSizeAnalysis
 * @brief Perform the Method Size Analysis pass.
 * Looks at various aspects of method sizes (for more information, refer to static_analysis_info.h).
 * APK Level:
 *  Keeps a cumulative count for the following:
 *    How many instructions (per 16 bit code item)
 *    # of Sub Tiny Methods.
 *    # of Tiny Methods.
 *    # of Sub Small Methods.
 *    # of Small Methods.
 *    # of Medium Methods.
 *    # of Large Methods.
 *    # of Too Large Methods.
 *
 * Method Level:
 *  Returns a static analysis method info bitmap to indicate ONE of the following:
 *    Is Sub Tiny Method?
 *    Is Tiny Method?
 *    Is Sub Small Method?
 *    Is Small Method?
 *    Is Medium Method?
 *    Is Large Method?
 */
class MethodSizeAnalysis : public StaticAnalysisPass {
 public:
    MethodSizeAnalysis() :
      StaticAnalysisPass("MethodSizeAnalysis"),
      static_analysis_method_cumulative_size_stats_(GetPassStatsInstance<StaticAnalysisMethodCumulativeSizeStats>()) {
    }

    /**
     * @fn PerformAnalysis(mirror::ArtMethod* method, const DexFile& dex_file)
     * @brief Performs the method size logistics analysis of the method to accumulate the number of
     *  1) how many 16 bit code item instructions , 2) Sub Tiny Methods, 3) Tiny Methods, 4) Sub Small Methods,
     *  5) Small Methods, 6) Medium Methods, 7) Large Methods, and 8) Too Large Methods.
     * @param method an ArtMethod pointer.
     * @param dex_file a dex file reference.
     * @param verifier a method verifier for our particular method.
     * @return A bit mask indicating:
     *  1)the size category ranging between Sub Tiny to Too Large.
     */
    uint32_t PerformAnalysis(StaticAnalysisMethodCumulativeStats* stats, mirror::ArtMethod* method, const DexFile* dex_file, verifier::MethodVerifier* verifier) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
      const DexFile::CodeItem* code_item = nullptr;
      if (method != nullptr) {
        code_item = dex_file->GetCodeItem(method->GetCodeItemOffset());
      } else if (verifier != nullptr) {
        code_item = verifier->CodeItem();
      }
      uint32_t static_analysis_method_info = kMethodNone;
      StaticAnalysisMethodCumulativeSizeStats* static_analysis_method_cumulative_size_stats = down_cast<StaticAnalysisMethodCumulativeSizeStats*>(stats);
      if (code_item != nullptr) {
        const uint32_t insns_size = code_item->insns_size_in_code_units_;
        static_analysis_method_cumulative_size_stats->size_of_all_methods_.FetchAndAddSequentiallyConsistent(insns_size);
        // Categorize the methods
        if (insns_size <= kSubTinyMethodLimit) {
          static_analysis_method_info |= kMethodSizeSubTiny;
          ++static_analysis_method_cumulative_size_stats->num_of_sub_tiny_methods_;
        } else if (insns_size <= kTinyMethodLimit) {
          static_analysis_method_info |= kMethodSizeTiny;
          ++static_analysis_method_cumulative_size_stats->num_of_tiny_methods_;
        } else if (insns_size <= kSubSmallMethodLimit) {
          static_analysis_method_info |= kMethodSizeSubSmall;
          ++static_analysis_method_cumulative_size_stats->num_of_sub_small_methods_;
        } else if (insns_size <= kSmallMethodLimit) {
          static_analysis_method_info |= kMethodSizeSmall;
          ++static_analysis_method_cumulative_size_stats->num_of_small_methods_;
        } else if (insns_size <= kMediumMethodLimit) {
          static_analysis_method_info |= kMethodSizeMedium;
          ++static_analysis_method_cumulative_size_stats->num_of_medium_methods_;
        } else if (insns_size < kLargeMethodLimit) {
          static_analysis_method_info |= kMethodSizeLarge;
          ++static_analysis_method_cumulative_size_stats->num_of_large_methods_;
        } else {
          static_analysis_method_info |= kMethodSizeTooLarge;
          ++static_analysis_method_cumulative_size_stats->num_of_too_large_methods_;
        }
      }
      return static_analysis_method_info;
    }

    /**
     * @fn DumpPassAnalysis(std::stringstream& string_stream) const
     * @brief Dumps stats for 1)instruction list size, 2) sub tiny methods, 3) tiny methods, 4) small methods
     * 5) medium methods, 6) large methods, 7) too large methods for debugging and data collection purposes.
     * @param[out] string_stream a concatenated stringstream containing the stats for the pass
     *  that can later be used in a LOG statement.
     * e.x. LOG(INFO) << my_string_stream.str().
     */
    void DumpPassAnalysis(std::stringstream& string_stream) const {
      string_stream << static_analysis_method_cumulative_size_stats_->size_of_all_methods_.LoadRelaxed() << " size of Methods in Dex File. "
          << static_analysis_method_cumulative_size_stats_->num_of_sub_tiny_methods_.LoadRelaxed() << " number of sub tiny methods. "
          << static_analysis_method_cumulative_size_stats_->num_of_tiny_methods_.LoadRelaxed() << " number of tiny methods. "
          << static_analysis_method_cumulative_size_stats_->num_of_sub_small_methods_.LoadRelaxed() << " number of sub small methods. "
          << static_analysis_method_cumulative_size_stats_->num_of_small_methods_.LoadRelaxed() << " number of small methods. "
          << static_analysis_method_cumulative_size_stats_->num_of_medium_methods_.LoadRelaxed() << " number of medium methods. "
          << static_analysis_method_cumulative_size_stats_->num_of_large_methods_.LoadRelaxed() << " number of large methods. "
          << static_analysis_method_cumulative_size_stats_->num_of_too_large_methods_.LoadRelaxed() << " number of too large methods. ";
    }

    StaticAnalysisMethodCumulativeStats* GetStats() const {
      return GetPassStatsInstance<StaticAnalysisMethodCumulativeSizeStats>();
    }

    StaticAnalysisMethodCumulativeSizeStats* static_analysis_method_cumulative_size_stats_;
};

/**
 * @class StaticAnalysisMethodCumulativeOpcodeStats
 * @brief Holder for the cumulative stats for opcode analysis.
 */
class StaticAnalysisMethodCumulativeOpcodeStats : public StaticAnalysisMethodCumulativeStats {
 public:
  StaticAnalysisMethodCumulativeOpcodeStats() {
    num_of_constant_assigns_.StoreRelaxed(0);
    num_of_method_invokes_.StoreRelaxed(0);
    num_of_unconditional_jumps_.StoreRelaxed(0);
    num_of_conditional_jumps_.StoreRelaxed(0);
    num_of_fp_instructions_.StoreRelaxed(0);
    num_of_methods_with_try_catch_.StoreRelaxed(0);
    num_of_throw_instructions_.StoreRelaxed(0);
    num_of_math_instructions_.StoreRelaxed(0);
    num_of_data_movement_setters_instructions_.StoreRelaxed(0);
    num_of_data_movement_getters_instructions_.StoreRelaxed(0);
  }
  /** Cumulative count of Constant Assign Dex Opcodes analyzed across all analyzed methods. */
  Atomic<uint32_t> num_of_constant_assigns_;
  /** Cumulative count of Method Invoke Dex Opcodes analyzed across all analyzed methods. */
  Atomic<uint32_t> num_of_method_invokes_;
  /** Cumulative count of Unconditional Jump Dex Opcodes analyzed across all analyzed methods. */
  Atomic<uint32_t> num_of_unconditional_jumps_;
  /** Cumulative count of Conditional Jump Dex Opcodes analyzed across all analyzed methods. */
  Atomic<uint32_t> num_of_conditional_jumps_;
  /** Cumulative count of Floating Point Dex Opcodes analyzed across all analyzed methods. */
  Atomic<uint32_t> num_of_fp_instructions_;
  /** Cumulative count of whether or not at least one try catch block existed in a method across all analyzed methods. */
  Atomic<uint32_t> num_of_methods_with_try_catch_;
  /** Cumulative count of Throw Dex Opcodes analyzed across all analyzed methods. */
  Atomic<uint32_t> num_of_throw_instructions_;
  /** Cumulative count of Arithmetic Dex Opcodes analyzed across all analyzed methods. */
  Atomic<uint32_t> num_of_math_instructions_;
  /** Cumulative count of Setters Dex Opcodes analyzed across all analyzed methods. */
  Atomic<uint32_t> num_of_data_movement_setters_instructions_;
  /** Cumulative count of Getters Dex Opcodes analyzed across all analyzed methods. */
  Atomic<uint32_t> num_of_data_movement_getters_instructions_;
};

/**
 * @class MethodOpcodeAnalysis
 * @brief Perform the Method Opcode Analysis pass.
 * Looks at the individual opcodes.
 * APK Level:
 *  Keeps a cumulative count for the following:
 *    Constant Assignments.
 *    Method Invokes.
 *    Unconditional Jumps.
 *    Conditional Jumps.
 *    Floating Point Instructions.
 *    Methods with at least one try catch block.
 *    Throw instructions.
 *    Getters.
 *    Setters.
 *
 * Method Level:
 *  Returns a static analysis method info bitmap with the following (for more information, refer to static_analysis_info.h):
 *    % of Getters.
 *    % of Setters.
 *    % of Invokes.
 *    Try / Catch block(s) Exist?
 *    % of Throw Instructions.
 *    % of Math Instructions.
 *    % of Constant Assignments.
 */
class MethodOpcodeAnalysis : public StaticAnalysisPass {
 public:
  MethodOpcodeAnalysis() :
      StaticAnalysisPass("MethodOpcodeAnalysis"),
      static_analysis_method_cumulative_opcode_stats_(GetPassStatsInstance<StaticAnalysisMethodCumulativeOpcodeStats>()) {
  }

  /**
   * @fn PerformAnalysis(mirror::ArtMethod* method, const DexFile& dex_file)
   * @brief Performs the method size logistics analysis of the method to accumulate the number of
   *  1) sub tiny methods, 2) tiny methods, 3) small methods 4) medium methods, 5) large methods,
   *  6) too large methods for debugging and data collection purposes.
   * @param method an ArtMethod pointer.
   * @param dex_file a dex file reference.
   * @param verifier a method verifier for our particular method.
   * @return A bit mask indicating:
   *  1) presence of try / catch blocks
   *  2) presences between either NONE, SMALL, MEDIUM, or LARGE for constant assignments.
   *  3) presences between either NONE, SMALL, MEDIUM, or LARGE for method invokes.
   *  4) presences between either NONE, SMALL, MEDIUM, or LARGE for setters.
   *  5) presences between either NONE, SMALL, MEDIUM, or LARGE for getters.
   *  6) presences between either NONE, SMALL, MEDIUM, or LARGE for math operations.
   *  7) presences between either NONE, SMALL, MEDIUM, or LARGE for throw instructions.
   */
  uint32_t PerformAnalysis(StaticAnalysisMethodCumulativeStats* stats, mirror::ArtMethod* method, const DexFile* dex_file, verifier::MethodVerifier* verifier) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    uint32_t static_analysis_method_info = kMethodNone;
    StaticAnalysisMethodCumulativeOpcodeStats* static_analysis_method_cumulative_opcode_stats = down_cast<StaticAnalysisMethodCumulativeOpcodeStats*>(stats);
    const DexFile::CodeItem* code_item = nullptr;
    if (method != nullptr) {
      code_item = dex_file->GetCodeItem(method->GetCodeItemOffset());
    } else if (verifier != nullptr) {
      code_item = verifier->CodeItem();
    }
    if (code_item != nullptr) {
      // Look for try / catch blocks.
      if (code_item->tries_size_ > 0) {
        static_analysis_method_info |= kMethodContainsTryCatch;
        ++static_analysis_method_cumulative_opcode_stats->num_of_methods_with_try_catch_;
      }
      const uint16_t* insns = code_item->insns_;
      const Instruction* insn = Instruction::At(insns);
      const uint32_t insns_size = code_item->insns_size_in_code_units_;
      /*
       * Local variables that represent the count on a per method basis
       * so that the ratio of a particular type can be determined within
       * the method.
       */
      uint32_t num_of_constant_assigns= 0, num_of_jumps = 0,
          numOfInvokes = 0, num_of_instructions= 0,
          num_of_math = 0, num_of_data_movement_getters = 0,
          num_of_data_movement_setters = 0;
      for (uint32_t dex_pc = 0; dex_pc < insns_size;
          insn = const_cast<Instruction*>(insn->Next()),
              dex_pc = insn->GetDexPc(insns)) {
        if (insn != nullptr) {
          ++num_of_instructions;
          // Constant, Math and Floating Point opcodes are not mutually exclusive
          if (IsConstantOperation(insn)) {
            ++num_of_constant_assigns;
            ++static_analysis_method_cumulative_opcode_stats->num_of_constant_assigns_;
          }
          if (IsMathOperation(insn)) {
            ++num_of_math;
            ++static_analysis_method_cumulative_opcode_stats->num_of_math_instructions_;
          }
          if (IsFloatingPointOperation(insn)) {
            ++static_analysis_method_cumulative_opcode_stats->num_of_fp_instructions_;
          } else if (IsUnconditionalJumpOperation(insn)) {
            ++num_of_jumps;
            ++static_analysis_method_cumulative_opcode_stats->num_of_unconditional_jumps_;
          } else if (IsConditionalJumpOperation(insn)) {
            ++num_of_jumps;
            ++static_analysis_method_cumulative_opcode_stats->num_of_conditional_jumps_;
          } else if (IsInvokeOperation(insn)) {
            ++numOfInvokes;
            ++static_analysis_method_cumulative_opcode_stats->num_of_method_invokes_;
          } else if (IsThrowOperation(insn)) {
            ++static_analysis_method_cumulative_opcode_stats->num_of_throw_instructions_;
          }
          if (IsSetterOperation(insn)) {
            ++num_of_data_movement_setters;
            ++static_analysis_method_cumulative_opcode_stats->num_of_data_movement_setters_instructions_;
          } else if (IsGetterOperation(insn)) {
            ++num_of_data_movement_getters;
            ++static_analysis_method_cumulative_opcode_stats->num_of_data_movement_getters_instructions_;
          }
        }
      }
      /**
       * Info Category: Getters.
       * Determine the ratio of Getters within the method.
       */
      static_analysis_method_info |= GetInfoBitValue(num_of_data_movement_getters, num_of_instructions,
              kMethodNone, kMethodContainsDataMovementsGettersSmall,
              kMethodContainsDataMovementsGettersMedium, kMethodContainsDataMovementsGettersLarge);
      /**
       * Info Category: Setters.
       * Determine the ratio of Setters within the method.
       */
      static_analysis_method_info |= GetInfoBitValue(num_of_data_movement_setters, num_of_instructions,
              kMethodNone, kMethodContainsDataMovementsSettersSmall,
              kMethodContainsDataMovementsSettersMedium, kMethodContainsDataMovementsSettersLarge);
      /**
       * Info Category: Arithmetic Instructions.
       * Determine the ratio of Arithmetic Instructions within the method.
       */
      static_analysis_method_info |= GetInfoBitValue(num_of_math, num_of_instructions,
                      kMethodNone, kMethodContainsArithmeticOperationsSmall,
                      kMethodContainsArithmeticOperationsMedium, kMethodContainsArithmeticOperationsLarge);
      /**
       * Info Category: Constant Assignment Instructions.
       * Determine the ratio of Constant Assignment Instructions within the method.
       */
      static_analysis_method_info |= GetInfoBitValue(num_of_constant_assigns, num_of_instructions,
                      kMethodNone, kMethodContainsConstantsSmall,
                      kMethodContainsConstantsMedium, kMethodContainsConstantsLarge);
      /**
       * Info Category: Invoke Instructions.
       * Determine the ratio of Invoke Instructions within the method.
       */
      static_analysis_method_info |= GetInfoBitValue(numOfInvokes, num_of_instructions,
                      kMethodNone, kMethodContainsInvokesSmall,
                      kMethodContainsInvokesMedium, kMethodContainsInvokesLarge);
      /**
       * Info Category: Jump / Control Flow Instructions (minus Invoke calls).
       * Determine the ratio of Jump / Control Flow Instructions within the method.
       */
      static_analysis_method_info |= GetInfoBitValue(num_of_jumps, num_of_instructions,
                      kMethodNone, kMethodContainsJumpsSmall,
                      kMethodContainsJumpsMedium, kMethodContainsJumpsLarge);
    }
      return static_analysis_method_info;
  }

  /**
   * @fn DumpPassAnalysis(std::stringstream& string_stream) const
   * @brief Dumps stats for 1) sub tiny methods, 2) tiny methods, 3) small methods
   * 4) medium methods, 5) large methods, 6) too large methods for debugging and data collection purposes.
   * @param[out] string_stream a concatenated stringstream containing the stats for the pass
   *  that can later be used in a LOG statement.
   * e.x. LOG(INFO) << my_string_stream.str().
   */
  void DumpPassAnalysis(std::stringstream& string_stream) const {
    string_stream
        << static_analysis_method_cumulative_opcode_stats_->num_of_constant_assigns_.LoadRelaxed() << " number of constant assigns. "
        << static_analysis_method_cumulative_opcode_stats_->num_of_unconditional_jumps_.LoadRelaxed() << " number of unconditional jumps. "
        << static_analysis_method_cumulative_opcode_stats_->num_of_conditional_jumps_.LoadRelaxed() << " number of conditional jumps. "
        << static_analysis_method_cumulative_opcode_stats_->num_of_method_invokes_.LoadRelaxed() << " number of method invokes. "
        << static_analysis_method_cumulative_opcode_stats_->num_of_fp_instructions_.LoadRelaxed() << " number of Floating Point Instructions. "
        << static_analysis_method_cumulative_opcode_stats_->num_of_methods_with_try_catch_.LoadRelaxed() << " number of methods with try catch. "
        << static_analysis_method_cumulative_opcode_stats_->num_of_throw_instructions_.LoadRelaxed() << " number of throw instructions. "
        << static_analysis_method_cumulative_opcode_stats_->num_of_math_instructions_.LoadRelaxed() << " number of math instructions. "
        << static_analysis_method_cumulative_opcode_stats_->num_of_data_movement_getters_instructions_.LoadRelaxed() << " number of getters. "
        << static_analysis_method_cumulative_opcode_stats_->num_of_data_movement_setters_instructions_.LoadRelaxed() << " number of setters. ";
  }

  StaticAnalysisMethodCumulativeStats* GetStats() const {
    return GetPassStatsInstance<StaticAnalysisMethodCumulativeOpcodeStats>();
  }

  StaticAnalysisMethodCumulativeOpcodeStats* static_analysis_method_cumulative_opcode_stats_;

 private:
  /**
   * @brief Returns true if opcode is a constant assignment operation.
   * @param insn The instruction that is being checked for its type.
   */
  bool IsConstantOperation(const Instruction* insn) const {
    switch (insn->Opcode()) {
      case Instruction::CONST:
      case Instruction::CONST_16:
      case Instruction::CONST_4:
      case Instruction::CONST_CLASS:
      case Instruction::CONST_STRING:
      case Instruction::CONST_STRING_JUMBO:
      case Instruction::CONST_WIDE:
      case Instruction::CONST_WIDE_16:
      case Instruction::CONST_WIDE_32:
      case Instruction::CONST_WIDE_HIGH16:
      // Below is also floating pointing.
      case Instruction::CONST_HIGH16:
        return true;
      default:
        return false;
    }
  }

  /**
   * @brief Returns true if opcode is a method invoke operation.
   * @param insn The instruction that is being checked for its type.
   */
  bool IsInvokeOperation(const Instruction* insn) const {
    return insn->IsInvoke();
  }

  /**
   * @brief Returns true if opcode is a unconditional jump operation.
   * @param insn The instruction that is being checked for its type.
   */
  bool IsUnconditionalJumpOperation(const Instruction* insn) const {
    return insn->IsUnconditional();
  }

  /**
   * @brief Returns true if opcode is a conditional jump operation.
   * @param insn The instruction that is being checked for its type.
   */
  bool IsConditionalJumpOperation(const Instruction* insn) const {
    return insn->IsBranch();
  }

  /**
   * @brief Returns true if opcode is a floating point operation.
   * @param insn The instruction that is being checked for its type.
   */
  bool IsFloatingPointOperation(const Instruction* insn) const {
    switch (insn->Opcode()) {
      case Instruction::ADD_DOUBLE:
      case Instruction::ADD_DOUBLE_2ADDR:
      case Instruction::ADD_FLOAT:
      case Instruction::ADD_FLOAT_2ADDR:
      case Instruction::DIV_DOUBLE:
      case Instruction::DIV_DOUBLE_2ADDR:
      case Instruction::DIV_FLOAT:
      case Instruction::DIV_FLOAT_2ADDR:
      case Instruction::DOUBLE_TO_FLOAT:
      case Instruction::DOUBLE_TO_INT:
      case Instruction::DOUBLE_TO_LONG:
      case Instruction::FLOAT_TO_DOUBLE:
      case Instruction::FLOAT_TO_INT:
      case Instruction::FLOAT_TO_LONG:
      case Instruction::INT_TO_DOUBLE:
      case Instruction::INT_TO_FLOAT:
      case Instruction::MUL_DOUBLE:
      case Instruction::MUL_DOUBLE_2ADDR:
      case Instruction::MUL_FLOAT:
      case Instruction::MUL_FLOAT_2ADDR:
      case Instruction::NEG_DOUBLE:
      case Instruction::NEG_FLOAT:
      case Instruction::REM_DOUBLE:
      case Instruction::REM_DOUBLE_2ADDR:
      case Instruction::REM_FLOAT:
      case Instruction::REM_FLOAT_2ADDR:
      case Instruction::SUB_DOUBLE:
      case Instruction::SUB_DOUBLE_2ADDR:
      case Instruction::SUB_FLOAT:
      case Instruction::SUB_FLOAT_2ADDR:
      case Instruction::CMPG_DOUBLE:
      case Instruction::CMPG_FLOAT:
      case Instruction::CMPL_DOUBLE:
      case Instruction::CMPL_FLOAT:
      // Below is also a constant assignment.
      case Instruction::CONST_HIGH16:
        return true;
      default:
        return false;
    }
  }

  /**
   * @brief Returns true if opcode is a throw instruction operation.
   * @param insn The instruction that is being checked for its type.
   */
  bool IsThrowOperation(const Instruction* insn) const {
    return insn->IsThrow();
  }

  /**
   * @brief Returns true if opcode is a math operation.
   * @param insn The instruction that is being checked for its type.
   */
  bool IsMathOperation(const Instruction* insn) const {
    switch (insn->Opcode()) {
      case Instruction::NEG_INT:
      case Instruction::NOT_INT:
      case Instruction::NEG_LONG:
      case Instruction::NOT_LONG:
      case Instruction::INT_TO_LONG:
      case Instruction::LONG_TO_INT:
      case Instruction::INT_TO_BYTE:
      case Instruction::INT_TO_CHAR:
      case Instruction::INT_TO_SHORT:
      case Instruction::ADD_INT:
      case Instruction::ADD_INT_2ADDR:
      case Instruction::ADD_INT_LIT16:
      case Instruction::ADD_INT_LIT8:
      case Instruction::SUB_INT:
      case Instruction::SUB_INT_2ADDR:
      case Instruction::RSUB_INT:
      case Instruction::RSUB_INT_LIT8:
      case Instruction::MUL_INT:
      case Instruction::MUL_INT_2ADDR:
      case Instruction::MUL_INT_LIT16:
      case Instruction::MUL_INT_LIT8:
      case Instruction::DIV_INT:
      case Instruction::DIV_INT_2ADDR:
      case Instruction::DIV_INT_LIT16:
      case Instruction::DIV_INT_LIT8:
      case Instruction::REM_INT:
      case Instruction::REM_INT_2ADDR:
      case Instruction::REM_INT_LIT16:
      case Instruction::REM_INT_LIT8:
      case Instruction::AND_INT:
      case Instruction::AND_INT_2ADDR:
      case Instruction::AND_INT_LIT16:
      case Instruction::AND_INT_LIT8:
      case Instruction::OR_INT:
      case Instruction::OR_INT_2ADDR:
      case Instruction::OR_INT_LIT16:
      case Instruction::OR_INT_LIT8:
      case Instruction::XOR_INT:
      case Instruction::XOR_INT_2ADDR:
      case Instruction::XOR_INT_LIT16:
      case Instruction::XOR_INT_LIT8:
      case Instruction::SHL_INT:
      case Instruction::SHL_INT_2ADDR:
      case Instruction::SHL_INT_LIT8:
      case Instruction::SHR_INT:
      case Instruction::SHR_INT_2ADDR:
      case Instruction::SHR_INT_LIT8:
      case Instruction::USHR_INT:
      case Instruction::USHR_INT_2ADDR:
      case Instruction::USHR_INT_LIT8:
      case Instruction::ADD_LONG:
      case Instruction::ADD_LONG_2ADDR:
      case Instruction::SUB_LONG:
      case Instruction::SUB_LONG_2ADDR:
      case Instruction::MUL_LONG:
      case Instruction::MUL_LONG_2ADDR:
      case Instruction::DIV_LONG:
      case Instruction::DIV_LONG_2ADDR:
      case Instruction::REM_LONG:
      case Instruction::REM_LONG_2ADDR:
      case Instruction::AND_LONG:
      case Instruction::AND_LONG_2ADDR:
      case Instruction::OR_LONG:
      case Instruction::OR_LONG_2ADDR:
      case Instruction::XOR_LONG:
      case Instruction::XOR_LONG_2ADDR:
      case Instruction::SHL_LONG:
      case Instruction::SHL_LONG_2ADDR:
      case Instruction::SHR_LONG:
      case Instruction::SHR_LONG_2ADDR:
      case Instruction::USHR_LONG:
      case Instruction::USHR_LONG_2ADDR:
      case Instruction::CMP_LONG:
      // Below instructions happen to also be floating point.
      case Instruction::ADD_DOUBLE:
      case Instruction::ADD_DOUBLE_2ADDR:
      case Instruction::ADD_FLOAT:
      case Instruction::ADD_FLOAT_2ADDR:
      case Instruction::DIV_DOUBLE:
      case Instruction::DIV_DOUBLE_2ADDR:
      case Instruction::DIV_FLOAT:
      case Instruction::DIV_FLOAT_2ADDR:
      case Instruction::DOUBLE_TO_FLOAT:
      case Instruction::DOUBLE_TO_INT:
      case Instruction::DOUBLE_TO_LONG:
      case Instruction::FLOAT_TO_DOUBLE:
      case Instruction::FLOAT_TO_INT:
      case Instruction::FLOAT_TO_LONG:
      case Instruction::INT_TO_DOUBLE:
      case Instruction::INT_TO_FLOAT:
      case Instruction::MUL_DOUBLE:
      case Instruction::MUL_DOUBLE_2ADDR:
      case Instruction::MUL_FLOAT:
      case Instruction::MUL_FLOAT_2ADDR:
      case Instruction::NEG_DOUBLE:
      case Instruction::NEG_FLOAT:
      case Instruction::REM_DOUBLE:
      case Instruction::REM_DOUBLE_2ADDR:
      case Instruction::REM_FLOAT:
      case Instruction::REM_FLOAT_2ADDR:
      case Instruction::SUB_DOUBLE:
      case Instruction::SUB_DOUBLE_2ADDR:
      case Instruction::SUB_FLOAT:
      case Instruction::SUB_FLOAT_2ADDR:
      case Instruction::CMPG_DOUBLE:
      case Instruction::CMPG_FLOAT:
      case Instruction::CMPL_DOUBLE:
      case Instruction::CMPL_FLOAT:
        return true;
      default:
        return false;
    }
  }

  /**
   * @brief Returns true if opcode is a setter operation.
   * @param insn The instruction that is being checked for its type.
   */
  bool IsSetterOperation(const Instruction* insn) const {
    switch (insn->Opcode()) {
      case Instruction::IPUT:
      case Instruction::IPUT_BOOLEAN:
      case Instruction::IPUT_BYTE:
      case Instruction::IPUT_CHAR:
      case Instruction::IPUT_OBJECT:
      case Instruction::IPUT_OBJECT_QUICK:
      case Instruction::IPUT_QUICK:
      case Instruction::IPUT_SHORT:
      case Instruction::IPUT_WIDE:
      case Instruction::IPUT_WIDE_QUICK:
      case Instruction::APUT:
      case Instruction::APUT_BOOLEAN:
      case Instruction::APUT_BYTE:
      case Instruction::APUT_CHAR:
      case Instruction::APUT_OBJECT:
      case Instruction::APUT_SHORT:
      case Instruction::APUT_WIDE:
      case Instruction::SPUT:
      case Instruction::SPUT_BOOLEAN:
      case Instruction::SPUT_BYTE:
      case Instruction::SPUT_CHAR:
      case Instruction::SPUT_OBJECT:
      case Instruction::SPUT_SHORT:
      case Instruction::SPUT_WIDE:
        return true;
      default:
        return false;
    }
  }

  /**
   * @brief Returns true if opcode is a getter operation.
   * @param insn The instruction that is being checked for its type.
   * */
  bool IsGetterOperation(const Instruction* insn) const {
    switch (insn->Opcode()) {
      case Instruction::IGET:
      case Instruction::IGET_BOOLEAN:
      case Instruction::IGET_BYTE:
      case Instruction::IGET_CHAR:
      case Instruction::IGET_OBJECT:
      case Instruction::IGET_OBJECT_QUICK:
      case Instruction::IGET_QUICK:
      case Instruction::IGET_SHORT:
      case Instruction::IGET_WIDE:
      case Instruction::IGET_WIDE_QUICK:
      case Instruction::AGET:
      case Instruction::AGET_BOOLEAN:
      case Instruction::AGET_BYTE:
      case Instruction::AGET_CHAR:
      case Instruction::AGET_OBJECT:
      case Instruction::AGET_SHORT:
      case Instruction::AGET_WIDE:
      case Instruction::SGET:
      case Instruction::SGET_BOOLEAN:
      case Instruction::SGET_BYTE:
      case Instruction::SGET_CHAR:
      case Instruction::SGET_OBJECT:
      case Instruction::SGET_SHORT:
      case Instruction::SGET_WIDE:
        return true;
      default:
        return false;
    }
  }
};
}  // namespace art
#endif  // ART_ANALYSIS_METHOD_STATIC_ANALYSIS_H_

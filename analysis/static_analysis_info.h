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

#ifndef ART_ANALYSIS_STATIC_ANALYSIS_INFO_H_
#define ART_ANALYSIS_STATIC_ANALYSIS_INFO_H_
#include <stdint.h>

namespace art {
/** Default No Static Analysis Info Found. */
static const uint32_t kMethodNone = 0x0000;

/**
 * Per Method Info.
 * There exist two types of static analysis info.
 * 1. Categorization / Generalization.
 * 2. Boolean [Exists / Does not exist].
 *
 * bits   0-2: Method Size.
 *  001: SubTiny, 010: Tiny, 011: SubSmall, 100: Small, 101: Medium, 110: Large, 111: Too Large.
 *  Refer to the limits below for more information.
 * bit      3: Try Catch Block Exist.
 * For the following categorizations:
 * 01: Less than 33% of that type of instruction exist per method.
 * 10: Greater than or equal to 33% but less than 66% of that type of instruction exist per method.
 * 11: Greater than or equal to 66% of that type of instruction exist per method.
 * bits   4-5: % of Arithmetic Operations.
 * bits   6-7: % of Constant Assignments.
 * bits   8-9: % of Getters.
 * bits 10-11: % of Setters.
 * bits 12-13: % of Invokes.
 * bits 14-15: % of Jumps.
 */

static const uint32_t kMethodSizeMask = 0x0007;
static const uint32_t kMethodSizeSubTiny = 0x0001;
static const uint32_t kMethodSizeTiny = 0x0002;
static const uint32_t kMethodSizeSubSmall = 0x0003;
static const uint32_t kMethodSizeSmall = 0x0004;
static const uint32_t kMethodSizeMedium = 0x0005;
static const uint32_t kMethodSizeLarge = 0x0006;
static const uint32_t kMethodSizeTooLarge = 0x0007;

static const uint32_t kMethodContainsTryCatch = 0x0008;
static const uint32_t kMethodContainsArithmeticOperationsMask = 0x0030;
static const uint32_t kMethodContainsArithmeticOperationsSmall = 0x0010;
static const uint32_t kMethodContainsArithmeticOperationsMedium = 0x0020;
static const uint32_t kMethodContainsArithmeticOperationsLarge = 0x0030;

static const uint32_t kMethodContainsConstantsMask = 0x00C0;
static const uint32_t kMethodContainsConstantsSmall = 0x0040;
static const uint32_t kMethodContainsConstantsMedium = 0x0080;
static const uint32_t kMethodContainsConstantsLarge = 0x00C0;

static const uint32_t kMethodContainsDataMovementsGettersMask = 0x0300;
static const uint32_t kMethodContainsDataMovementsGettersSmall = 0x0100;
static const uint32_t kMethodContainsDataMovementsGettersMedium = 0x0200;
static const uint32_t kMethodContainsDataMovementsGettersLarge = 0x0300;

static const uint32_t kMethodContainsDataMovementsSettersMask = 0x0C00;
static const uint32_t kMethodContainsDataMovementsSettersSmall = 0x0400;
static const uint32_t kMethodContainsDataMovementsSettersMedium = 0x0800;
static const uint32_t kMethodContainsDataMovementsSettersLarge = 0x0C00;

static const uint32_t kMethodContainsInvokesMask = 0x03000;
static const uint32_t kMethodContainsInvokesSmall = 0x01000;
static const uint32_t kMethodContainsInvokesMedium = 0x02000;
static const uint32_t kMethodContainsInvokesLarge = 0x03000;

static const uint32_t kMethodContainsJumpsMask = 0x0C000;
static const uint32_t kMethodContainsJumpsSmall = 0x04000;
static const uint32_t kMethodContainsJumpsMedium = 0x08000;
static const uint32_t kMethodContainsJumpsLarge = 0x0C000;

/**
 * Method Size Limits.
 * These are different method sizes.
 * Each unit of size represents one 16 bit code unit, also known as an insn.
 * For more information: https://source.android.com/devices/tech/dalvik/dex-format.html
 */

static const uint32_t kSubTinyMethodLimit = 0x0008;   // 8
static const uint32_t kTinyMethodLimit = 0x0010;      // 16
static const uint32_t kSubSmallMethodLimit = 0x0080;  // 128
static const uint32_t kSmallMethodLimit = 0x0100;     // 256
static const uint32_t kMediumMethodLimit = 0x1000;    // 4096
static const uint32_t kLargeMethodLimit = 0x10000;    // 65536

/**
 * Method Static Analysis Info Presence.
 * These are the minimum percentages that help categorize the info a particular size
 *  within StaticAnalysisInfoSize.
 */

static const float kLargeStaticAnalysisInfoMin = 0.66;   // 66%
static const float kMediumStaticAnalysisInfoMin = 0.33;  // 33%
static const float kSmallStaticAnalysisInfoMin = 0.00;   // 0%

/** @brief The different sizes of analysis info. */
enum class StaticAnalysisInfoSize { NONE, SMALL, MEDIUM, LARGE };
}  // namespace art

#endif  // ART_ANALYSIS_STATIC_ANALYSIS_INFO_H_


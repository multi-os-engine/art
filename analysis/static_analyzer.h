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

#ifndef ART_ANALYSIS_STATIC_ANALYZER_H_
#define ART_ANALYSIS_STATIC_ANALYZER_H_
#include "base/macros.h"
#include "base/mutex.h"
#include "base/stl_util.h"
#include "dex_instruction-inl.h"
#include "dex/pass_driver.h"
#include "mirror/art_method.h"
#include "safe_map.h"
#include "static_analysis_info.h"
#include "static_analysis_pass.h"
#include "static_analysis_pass_driver.h"
#include "verifier/method_verifier.h"
#include "verifier/method_verifier-inl.h"

namespace art {

namespace verifier {
class MethodVerifier;
}  // namespace verifier

class StaticAnalyzer {
  public:
    StaticAnalyzer();
    ~StaticAnalyzer();
    /**
     * Query function.
     * These functions provide the ability to perform queries on the static analysis method info bitmap stored.
     */

    /**
     * @brief Returns whether the method is classified as a particular size.
     * @return True if the number of 16 bit instructions (insns_size_in_code_units_)
     *  in a code_item is less than a certain limit. If a method is within a certain limit
     *  it will be represented as the corresponding bitmap which is in static_analysis_info.h.
     *  Otherwise, false.
     * @param method The ArtMethod* used as the key to search the static analysis method info table for the
     *  method's corresponding info bit map.
     * @param method_size_bitmap The bitmap corresponding to a particular size that we are searching for within
     *  the entry in the static analysis method info table.
     */
    bool IsMethodSizeIn(mirror::ArtMethod* method, uint32_t method_size_bitmap) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    bool IsMethodSizeIn(const DexFile::MethodId* method_id, uint32_t method_size_bitmap) const SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    // TODO Add more query functions

    typedef SafeMap<const DexFile::MethodId*, uint32_t*> StaticAnalysisMethodInfoTable;
    // All method references with method info.
    mutable Mutex static_analysis_methods_info_lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;
    StaticAnalysisMethodInfoTable static_analysis_methods_info_ GUARDED_BY(static_analysis_methods_info_lock_);

    /**
     * @brief Public call that analyzes the passed in ArtMethod* and analyzes it over various passes
     *  Each pass returns a bitmap containing method info. Once the complete bitmap is assembled representing
     *  all the method information collected, it is stored in a map "static_analysis_methods_info_".
     * @param method The ArtMethod to be analyzed.
     * @param dex_file The dex file.
     */
    void AnalyzeMethod(mirror::ArtMethod* method,
        const DexFile& dex_file)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    void AnalyzeMethod(verifier::MethodVerifier* verifier)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

    /**
     * @brief Cleanup any allocated unecessary memory in case no per-method data was found
     *  after analyzing a method.
     */
    void StaticAnalysisMemoryCleanup(uint32_t* static_analysis_method_info, const DexFile::MethodId& method_id);

    /**
     * @brief Concatenates the stats of each pass by calling each
     *  pass's DumpPassAnalysis method and returns the concatenated string.
     *  Useful if one wants to get the stats to make for their own logging.
     */
    const std::string DumpAnalysis();

    /**
     * @brief Concatenates the stats of each pass by calling each
     *  pass's DumpPassAnalysis method and logs the concatenated string.
     */
    void LogAnalysis();

  private:
    /**
     * @brief Searches for and returns a bitmap with info for a particular ArtMethod*.
     * @param method The MethodId used as the key to search the static analysis method info table for the
     *  particular bit map.
     */
    uint32_t* GetStaticAnalysisMethodInfo(const DexFile::MethodId& method_id) const
    LOCKS_EXCLUDED(static_analysis_methods_info_lock_);

    /**
     * @brief Reserves a spot in the table for the given MethodId.
     *  Returns nullptr if a space already exists, signifying that the method is analyzed / being analyzed already.
     *  Returns a new uint32_t* if a space was successfully reserved and should be used to pass on to the pass driver.
     * @param method_id The MethodId used as the key to search the static analysis method info table for the
     *  particular bit map.
     */
    uint32_t* ReserveSpotStaticAnalysisMethodInfo(const DexFile::MethodId& method_id)
    LOCKS_EXCLUDED(static_analysis_methods_info_lock_);

    DISALLOW_COPY_AND_ASSIGN(StaticAnalyzer);  // disallow copy constructor
};
}  // namespace art
#endif  // ART_ANALYSIS_STATIC_ANALYZER_H_

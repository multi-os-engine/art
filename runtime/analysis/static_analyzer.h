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

#ifndef ART_RUNTIME_ANALYSIS_STATIC_ANALYZER_H_
#define ART_RUNTIME_ANALYSIS_STATIC_ANALYZER_H_
#include "analysis/static_analysis_info.h"
#include "analysis/static_analysis_pass.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "base/stl_util.h"
#include "dex_instruction-inl.h"
#include "mirror/art_method.h"
#include "safe_map.h"
#include "thread.h"

namespace art {
class StaticAnalyzer {
  public:
    StaticAnalyzer();
    ~StaticAnalyzer();

    /**
     * @brief Adds a new StaticAnalysisPass that should be used when analyzing the methods.
     *  The pass will not be added if a pass of the same type is already included.
     * @param new_pass The pass to add the list of passes that will analyze each method.
     */
    void InsertPass(StaticAnalysisPass* new_pass);
    /**
     * @brief Creates the default passes and stores them so they will be used when analyzing the methods.
     */
    void CreatePasses();
    /**
     * @brief Searches for whether or not a particular pass has been queued up to perform analysis
     *  on a method by searching for it by name.
     * @name the name of the pass so it can be searched by its name.
     */
    StaticAnalysisPass* GetPass(const char* name);

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
    bool IsMethodSizeIn(mirror::ArtMethod* method, uint32_t method_size_bitmap) const;
    // TODO Add more query functions

    typedef SafeMap<mirror::ArtMethod*, uint32_t*> StaticAnalysisMethodInfoTable;
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

    /**
     * @brief Allows the developer to add timing (NanoTime suggested) around a particular feature and
     *  prepend up to two timings to the beginning of the string that will be returned with the all
     *  stats from each pass via DumpPassAnalysis.
     * @param first_time The timing to complete the first event.
     * @param second_time The timing to complete the second event.
     */
    const std::string DumpTimedAnalysis(uint32_t first_time, uint32_t second_time);
    /**
     * @brief Concatenates the stats of each pass by calling each
     *  pass's DumpPassAnalysis method and returns the concatenated string.
     *  Useful if one wants to get the stats to make for their own logging.
     */
    const std::string DumpAnalysis();
    /**
     * @brief Concatenates the stats of each pass by calling each pass's
     *  DumpPassAnalysis method. Allows the developer to also prepend up to two timings
     *  to the concatenated string and logs the concatenated string.
     * @param first_time The timing to complete the first event.
     * @param second_time The timing to complete the second event.
     */
    void LogTimedAnalysis(uint32_t first_time, uint32_t second_time);
    /**
     * @brief Concatenates the stats of each pass by calling each
     *  pass's DumpPassAnalysis method and logs the concatenated string.
     */
    void LogAnalysis();

  private:
    /** @brief List of static analysis passes: provides the order to execute the passes. */
    std::vector<StaticAnalysisPass*> pass_list_;

    /**
     * @brief Searches for and returns a bitmap with info for a particular ArtMethod*.
     * @param method The ArtMethod* used as the key to search the static analysis method info table for the
     *  particular bit map.
     */
    uint32_t* GetStaticAnalysisMethodInfo(mirror::ArtMethod* method) const
    LOCKS_EXCLUDED(static_analysis_methods_info_lock_);

    DISALLOW_COPY_AND_ASSIGN(StaticAnalyzer);  // disallow copy constructor
};
}  // namespace art
#endif  // ART_RUNTIME_ANALYSIS_STATIC_ANALYZER_H_

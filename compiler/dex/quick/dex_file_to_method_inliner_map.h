/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ART_COMPILER_DEX_QUICK_DEX_FILE_TO_METHOD_INLINER_MAP_H_
#define ART_COMPILER_DEX_QUICK_DEX_FILE_TO_METHOD_INLINER_MAP_H_

#include <map>
#include <vector>
#include "base/macros.h"
#include "base/mutex.h"

#include "dex/quick/dex_file_method_inliner.h"

namespace art {

class CompilerDriver;
class DexFile;

/**
 * Map each DexFile to its DexFileMethodInliner.
 *
 * The method inliner is created and initialized the first time it's requested
 * for a particular DexFile.
 */
class DexFileToMethodInlinerMap {
  public:
    DexFileToMethodInlinerMap();
    ~DexFileToMethodInlinerMap();

    DexFileMethodInliner* GetMethodInliner(const DexFile* dex_file) NO_THREAD_SAFETY_ANALYSIS;
        // TODO: There is an irregular non-scoped use of locks that defeats annotalysis with -O0.
        // Fix the NO_THREAD_SAFETY_ANALYSIS when this works and add the appropriate LOCKS_EXCLUDED.

    /**
     * Create raw data for inline references.
     * This should be called after all methods have been compiled and we need to record
     * the inline references in the oat file for the debugger.
     *
     * @param dex_files dex files vector for conversion of DexFile pointers into indexes.
     */
    const std::vector<uint8_t>* CreateInlineRefs(const std::vector<const DexFile*>& dex_files);

  private:
    ReaderWriterMutex lock_;
    std::map<const DexFile*, DexFileMethodInliner*> inliners_ GUARDED_BY(lock_);

    DISALLOW_COPY_AND_ASSIGN(DexFileToMethodInlinerMap);
};

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_DEX_FILE_TO_METHOD_INLINER_MAP_H_

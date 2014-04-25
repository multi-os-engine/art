/*
 * Copyright (C) 2014 Intel Corporation
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

#ifndef ART_COMPILER_DEX_LOOP_FORMATION_H_
#define ART_COMPILER_DEX_LOOP_FORMATION_H_

#include "pass.h"

namespace art {

    /**
     * @brief The current pass finds the loop entry and creates the loop hierarchy.
     */
    class FindLoops:public Pass {
      public:
        FindLoops(void):Pass("FindLoops", kNoNodes) {
        }

        virtual void Start(CompilationUnit *c_unit) const;
    };

    /**
     * @brief Form loops adds the preheader and exit blocks to the loops found.
     */
    class FormLoops:public Pass {
      public:
        FormLoops(void):Pass("FormLoops", kNoNodes) {
        }

        virtual void Start(CompilationUnit *c_unit) const;

      private:
        static bool Worker(LoopInformation* loop_info, void* data);
        static void InsertPreLoopHeader(MIRGraph* mir_graph, LoopInformation* loop_info, BasicBlock* entry);
        static BasicBlock* HandleTopLoop(MIRGraph* mir_graph, LoopInformation* loop_info);
        static bool IsTransformationRequired(const BitVector* not_loop, const BasicBlock* entry);
    };

}  // namespace art

#endif  // ART_COMPILER_DEX_LOOP_FORMATION_H_

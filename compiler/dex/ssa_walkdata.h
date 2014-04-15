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
#ifndef ART_COMPILER_DEX_SSA_WALKDATA_H_
#define ART_COMPILER_DEX_SSA_WALKDATA_H_

#include <map>
#include "compiler_internals.h"
#include "mir_graph.h"

namespace art {

    /**
     * @class WalkDataNoDefine
     * @brief Used as a helper structure to handle SSA registers without a definition during the parsing.
     */
    struct WalkDataNoDefine {
        MIR* mir_;   /**< @brief the MIR containing the use without a definition. */
        int index_;  /**< @brief the index in the ssaRep->uses array for the SSA register. */
    };

    /**
     * @class SSAWalkData
     * @brief SSAWalkData contains any data required inter BasicBlock.
     */
    class SSAWalkData {
    public:
        explicit SSAWalkData(MIRGraph *mir_graph);
        ~SSAWalkData(void);

        UsedChain* GetUsedChain(void);

        /**
         * @brief Get last chain node for a particular SSA register.
         * @return the last chain node for the value SSA register or nullptr.
         */
        UsedChain* GetLastChain(int ssa_reg);

        /**
         * @brief Set the last chain for a given SSA register.
         */
        void SetLastChain(UsedChain* chain, int ssa_reg);

        /**
         * @brief Associate a defined register and the instruction.
         */
        void SetDefinition(MIR* insn, int ssa_reg);

        /**
         * @brief Get the instruction containing the definition.
         * @return the MIR containing the definition, nullptr if none found.
         */
        MIR* GetDefinition(int ssa_reg) const;

        /**
         * @brief Handle the SSA registers without a definition during the parsing.
         */
        void HandleNoDefinitions(void);

        /**
         * @brief Add a SSA register that does not have a definition during the parsing.
         * @param mir the MIR containing the SSA register without definition.
         * @param idx the index in the ssaRep->uses array for the register.
         */
        void AddNoDefine(MIR* mir, int idx);

        /**
         * @brief update Def chain with new use.
         * @param use_idx the uses index.
         * @param used the MIR corresponding to Use.
         * @param defined the MIR corresponding to Def.
         */
        void AddUseToDefChain(int use_idx, MIR* used, MIR* defined);

    protected:
        /** @brief Association SSA register <-> where it is defined. */
        std::map<int, MIR*> definitions_;

        /** @brief Association SSA register <-> the last use chain node. */
        std::map<int, UsedChain*> last_chain_;

        /** @brief Free chain node list. */
        UsedChain** free_chains_list_;

        /** @brief Current free chain node. */
        UsedChain* free_chains_;

        /** @brief Any MIR not having a definition during the parsing. */
        std::vector<WalkDataNoDefine> no_define_;

        /** @brief Local copy of a pointer to the MIRGraph arena. */
        ArenaAllocator* arena_;
    };
}  //  namespace art

#endif  // ART_COMPILER_DEX_SSA_WALKDATA_H_

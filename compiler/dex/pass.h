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

#ifndef ART_COMPILER_DEX_PASS_H_
#define ART_COMPILER_DEX_PASS_H_

#include <string>

namespace art {

    // Forward declarations.
    class BasicBlock;
    class CompilationUnit;
    class Pass;

    /**
     * @brief OptimizationFlag is an enumeration to perform certain tasks for a given pass.
     * @details Each enum should be a power of 2 to be correctly used.
     */
    enum OptimizationFlag {
    };

    enum DataFlowAnalysisMode {
        kAllNodes = 0,                           // All nodes.
        kPreOrderDFSTraversal,                   // Depth-First-Search / Pre-Order.
        kRepeatingPreOrderDFSTraversal,          // Depth-First-Search / Repeating Pre-Order.
        kReversePostOrderDFSTraversal,           // Depth-First-Search / Reverse Post-Order.
        kRepeatingPostOrderDFSTraversal,         // Depth-First-Search / Repeating Post-Order.
        kRepeatingReversePostOrderDFSTraversal,  // Depth-First-Search / Repeating Reverse Post-Order.
        kPostOrderDOMTraversal,                  // Dominator tree / Post-Order.
    };

    /**
     * @class Pass
     * @brief Pass is the Pass structure for the optimizations.
     * The following structure has the different optimization passes that we are going to do.
     */
    class Pass {
        protected:
            /** @brief The pass name. */
            std::string passName;

            /** @brief Type of traversal. */
            DataFlowAnalysisMode traversalType;

            /** @brief Should the driver free the pass?  */
            bool freeByDriver;

            /** @brief Flags for additional directives. */
            unsigned int flags;

            /** @brief CFG Dump Folder. */
            std::string dumpCFGFolder;

        public:
            /**
             * @brief Constructor.
             */
            Pass(void);

            /**
             * @brief Destructor.
             */
            virtual ~Pass(void) {}

            /**
             * @brief Get the Pass name.
             * @return the name.
             */
            virtual const std::string &getName(void) const {return passName;}

            /**
             * @brief Get the traversal type.
             * @return the traversal type.
             */
            virtual DataFlowAnalysisMode getTraversal(void) const {return traversalType;}

            /**
             * @brief Get the Pass' flags.
             * @param flag the flag we want to test.
             * @return whether the flag is set.
             */
            virtual bool getFlag(OptimizationFlag flag) const {return (flags & flag);}


            /**
             * @brief Gate for the pass, taking the CompilationUnit and the pass information.
             * @param cUnit the CompilationUnit.
             */
            virtual bool gate(const CompilationUnit *cUnit) const;

            /**
             * @brief Start of the pass function.
             * @param cUnit the CompilationUnit.
             */
            virtual void start(CompilationUnit *cUnit) const;

            /**
             * @brief End of the pass function.
             * @param cUnit the CompilationUnit.
             */
            virtual void end(CompilationUnit *cUnit) const;

            /**
             * @brief Actually walk the BasicBlocks following a particular traversal type.
             * @param cUnit the CompilationUnit.
             * @param bb the BasicBlock.
             * @return whether or not there is a change when walking the BasicBlock
             */
            virtual bool walkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const;

            /**
             * @brief Should the PassDriver free the pass?
             * @return whether the PassDriver should free the pass.
             */
            bool shouldDriverFree(void) const {return freeByDriver;}

            /**
             * @brief Get the folder where to dump the CFG.
             * @return the folder where to dump the CFG.
             */
            const std::string& getDumpCFGFolder(void) const {return dumpCFGFolder;}
    };
}  // namespace art
#endif  // ART_COMPILER_DEX_PASS_H_

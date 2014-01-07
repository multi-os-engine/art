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

#ifndef ART_COMPILER_DEX_PASS_DRIVER_H_
#define ART_COMPILER_DEX_PASS_DRIVER_H_

#include <list>
#include <map>

#include "pass.h"

// Forward Declarations.
class CompilationUnit;
class Pass;

namespace art {
  class PassDriver {
    protected:
      /** @brief The Pass Map: contains name -> pass for quick lookup. */
      std::map<std::string, Pass *> passMap;

      /** @brief List of passes. */
      std::list<Pass *> passList;

      /** @brief The CompilationUnit on which this pass operates. */
      CompilationUnit* const cu;

      /** @brief Dump CFG base folder. */
      std::string dumpCFGFolder;

      /**
       * @brief Add the passes to the map and list.
       * @param passes array of passes.
       * @param nbr number of passes.
       */
      void addPassArray(Pass **passes, unsigned int nbr);

      /**
       * @brief Create the Pass list.
       */
      void createPasses(void);

    public:
      /**
       * @brief Construct a PassDriver.
       * @param cu The CompilationUnit on which the pass operates.
       * @param createDefaultPasses do we create the default passes (default: true).
       */
      explicit PassDriver(CompilationUnit* const cu, bool createDefaultPasses = true);

      /**
       * @brief Destructor.
       */
      ~PassDriver(void);

      /**
       * @brief Insert a Pass.
       * @param newPass the new Pass.
       * @param warnOverride warn if the name of the Pass is already used.
       */
      void insertPass(Pass *newPass, bool warnOverride = true);

      /**
       * @brief Run a pass using the name.
       * @param cUnit the CompilationUnit.
       * @param passName the Pass name.
       * @return whether the pass was applied.
       */
      bool runPass(CompilationUnit *cUnit, const std::string &passName);

      /**
       * @brief Run a pass using the Pass itself.
       * @param cUnit the CompilationUnit.
       * @param curPass the Pass.
       * @param timeSplit do we want a time split request(default: false)?
       * @return whether the pass was applied.
       */
      bool runPass(CompilationUnit *cUnit, Pass *curPass, bool timeSplit = false);

      /**
       * @brief Launch the PassDriver on the CompilationUnit and execute each Pass.
       * @param cUnit the CompilationUnit.
       */
      void launch();

      /**
       * @brief Handle any pass flag that requires clean-up.
       * @param cUnit the CompilationUnit.
       * @param pass the Pass.
       */
      void handlePassFlag(CompilationUnit *cUnit, Pass *pass);

      /**
       * @brief Apply a patch: perform start/work/end functions.
       * @param cUnit the CompilationUnit.
       * @param pass the Pass.
       */
      void applyPass(CompilationUnit *cUnit, Pass *pass);

      /**
       * @brief Dispatch a patch: walk the BasicBlocks depending on the traversal mode
       * @param cUnit the CompilationUnit.
       * @param pass the Pass.
       */
      void dispatchPass(CompilationUnit *cUnit, Pass *pass);

      /**
       * @brief Print the Pass names.
       */
      void printPassNames(void) const;

      /**
       * @brief Get a Pass.
       * @param name the Pass' name.
       * @return the Pass if found, 0 otherwise.
       */
      Pass *getPass(const std::string &name) const;

      /**
       * @brief Get the folder where to dump the pass CFGs.
       * @return the folder where to dump the pass CFGs.
       */
      const std::string& getDumpCFGFolder(void) const {return dumpCFGFolder;}
  };
}  // namespace art
#endif  // ART_COMPILER_DEX_PASS_DRIVER_H_

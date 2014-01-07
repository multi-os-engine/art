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

#include "compiler_internals.h"
#include "pass.h"

namespace art {
    // Constructor and Destructor.
    Pass::Pass(void) {
        passName = "Pass without name";
        traversalType = kAllNodes;
        flags = 0;
        freeByDriver = false;
        dumpCFGFolder = "";
    }

    bool Pass::Gate(const CompilationUnit *cUnit) const {
      // Unused parameter.
      UNUSED (cUnit);

      // Base class says yes.
      return true;
    }

    void Pass::Start(CompilationUnit *cUnit) const {
      // Unused parameter.
      UNUSED (cUnit);
    }

    void Pass::End(CompilationUnit *cUnit) const {
      // Unused parameter.
      UNUSED (cUnit);
    }

    bool Pass::WalkBasicBlocks(CompilationUnit *cUnit, BasicBlock *bb) const {
      // Unused parameters.
      UNUSED (cUnit);
      UNUSED (bb);

      // BasicBlock did not change.
      return false;
    }

}  // namespace art

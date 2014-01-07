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

#include <dlfcn.h>

#include "bb_optimizations.h"
#include "compiler_internals.h"
#include "dataflow_iterator.h"
#include "dataflow_iterator-inl.h"
#include "pass.h"
#include "pass_driver.h"

namespace art {
  PassDriver::PassDriver(CompilationUnit* const cu, bool createDefaultPasses):cu(cu) {
    dumpCFGFolder = "/sdcard/";

    // If need be, create the default passes.
    if (createDefaultPasses == true) {
      CreatePasses();
    }
  }

  PassDriver::~PassDriver(void) {
    // Clear the map: done to remove any chance of having a pointer after freeing below
    passMap.clear();

    for (std::list<Pass *>::iterator it = passList.begin(); it != passList.end(); it++) {
      Pass *curPass = *it;

      if (curPass->ShouldDriverFree() == true) {
        delete curPass, curPass = 0;
      }
    }
  }

  void PassDriver::InsertPass(Pass *newPass, bool warnOverride) {
    assert(newPass != 0);

    // Get name here to not do it all over the method.
    const std::string &name = newPass->GetName();

    // Do we want to warn the user about squashing a pass?
    if (warnOverride == false) {
      std::map<std::string, Pass *>::iterator it = passMap.find(name);

      if (it != passMap.end()) {
        LOG(INFO) << "Pass name " << name << " already used, overwriting pass";
      }
    }

    // Now add to map and list.
    passMap[name] = newPass;
    passList.push_back(newPass);
  }

  void PassDriver::CreatePasses(void) {
    // Create the pass list
    static Pass *passes[] = {
      new CodeLayout(),
      new SSATransformation(),
      new ConstantPropagation(),
      new InitRegLocations(),
      new MethodUseCount(),
      new NullCheckEliminationAndTypeInferenceInit(),
      new NullCheckEliminationAndTypeInference(),
      new BBCombine(),
      new BBOptimizations(),
    };

    // Get number of elements in the array.
    unsigned int nbr = (sizeof(passes) / sizeof(passes[0]));

    // Insert each pass into the map and into the list via the InsertPass method:
    //   - Map is used for the lookup
    //   - List is used for the pass walk
    for (unsigned int i = 0; i < nbr; i++) {
      InsertPass(passes[i]);
    }
  }

  void PassDriver::HandlePassFlag(CompilationUnit *cUnit, Pass *pass) {
    // Unused parameters for the moment.
    UNUSED(cUnit);
    UNUSED(pass);
  }

  void PassDriver::DispatchPass(CompilationUnit *cUnit, Pass *curPass) {
    DataflowIterator *iterator = 0;

    LOG(DEBUG) << "Dispatching " << curPass->GetName().c_str();

    MIRGraph *mir_graph = cUnit->mir_graph.get();

    // Let us start by getting the right iterator.
    DataFlowAnalysisMode mode = curPass->GetTraversal();

    switch (mode) {
      case kPreOrderDFSTraversal:
        iterator = new PreOrderDfsIterator(mir_graph);
        break;
      case kRepeatingPreOrderDFSTraversal:
        iterator = new RepeatingPreOrderDfsIterator(mir_graph);
        break;
      case kRepeatingPostOrderDFSTraversal:
        iterator = new RepeatingPostOrderDfsIterator(mir_graph);
        break;
      case kReversePostOrderDFSTraversal:
        iterator = new ReversePostOrderDfsIterator(mir_graph);
        break;
      case kRepeatingReversePostOrderDFSTraversal:
        iterator = new RepeatingReversePostOrderDfsIterator(mir_graph);
        break;
      case kPostOrderDOMTraversal:
        iterator = new PostOrderDOMIterator(mir_graph);
        break;
      case kAllNodes:
        iterator = new AllNodesIterator(mir_graph);
        break;
      default:
        LOG(DEBUG) << "Iterator mode not handled in dispatcher: " << mode;
        return;
    }

    // Paranoid: Check the iterator before walking the BasicBlocks.
    assert(iterator != 0);

    bool change = false;
    for (BasicBlock *bb = iterator->Next(change); bb != 0; bb = iterator->Next(change)) {
      change = curPass->WalkBasicBlocks(cUnit, bb);
    }

    // We are done with the iterator.
    delete iterator, iterator = 0;
  }

  void PassDriver::ApplyPass(CompilationUnit *cUnit, Pass *curPass) {
    curPass->Start(cUnit);
    DispatchPass(cUnit, curPass);
    curPass->End(cUnit);
  }

  bool PassDriver::RunPass(CompilationUnit *cUnit, Pass *curPass, bool timeSplit) {
    // Paranoid: cUnit or curPass cannot be 0, and the pass should have a name.
    if (cUnit == 0 || curPass == 0 || curPass->GetName() == "") {
      return false;
    }

    // Do we perform a time split
    if (timeSplit == true) {
      std::string name = "MIROpt:";
      name += curPass->GetName();
      cUnit->NewTimingSplit(name.c_str());
    }

    // Check the pass gate first.
    bool shouldApplyPass = curPass->Gate(cUnit);

    if (shouldApplyPass == true) {
      // Applying the pass: first start, doWork, and end calls.
      ApplyPass(cUnit, curPass);

      // Clean up if need be.
      HandlePassFlag(cUnit, curPass);

      // Do we want to log it?
      if ((cUnit->enable_debug & (1 << kDebugDumpCFG)) != 0) {
        // Do we have a pass folder?
        const std::string &passFolder = curPass->GetDumpCFGFolder();

        if (passFolder != "") {
          // Create directory prefix.
          std::string prefix = GetDumpCFGFolder();
          prefix += passFolder;
          prefix += "/";

          cUnit->mir_graph->DumpCFG(prefix.c_str(), false);
        }
      }
    }

    // If the pass gate passed, we can declare success.
    return shouldApplyPass;
  }

  bool PassDriver::RunPass(CompilationUnit *cUnit, const std::string &passName) {
    // Paranoid: cUnit cannot be 0 and we need a pass name.
    if (cUnit == 0 || passName == "") {
      return false;
    }

    Pass *curPass = GetPass(passName);

    if (curPass != 0) {
      return RunPass(cUnit, curPass);
    }

    // Return false, we did not find the pass.
    return false;
  }

  void PassDriver::Launch(void) {
    for (std::list<Pass *>::iterator it = passList.begin(); it != passList.end(); it++) {
      Pass *curPass = *it;
      RunPass(cu, curPass, true);
    }
  }

  void PassDriver::PrintPassNames(void) const {
    LOG(INFO) << "Loop Passes are:";

    for (std::list<Pass *>::const_iterator it = passList.begin(); it != passList.end(); it++) {
      const Pass *curPass = *it;
      LOG(INFO) << "\t-" << curPass->GetName();
    }
  }

  Pass *PassDriver::GetPass(const std::string &name) const {
    std::map<std::string, Pass *>::const_iterator it = passMap.find(name);

    if (it != passMap.end()) {
      return it->second;
    }

    return 0;
  }
}  // namespace art

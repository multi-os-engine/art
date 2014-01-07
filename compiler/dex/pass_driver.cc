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
      createPasses();
    }
  }

  PassDriver::~PassDriver(void) {
    // Clear the map: done to remove any chance of having a pointer after freeing below
    passMap.clear();

    // Now go through the list.
    for (std::list<Pass *>::iterator it = passList.begin(); it != passList.end(); it++) {
      Pass *curPass = *it;

      // Are we responsible for freeing it?
      if (curPass->shouldDriverFree() == true) {
        delete curPass, curPass = 0;
      }
    }
  }

  void PassDriver::insertPass(Pass *newPass, bool warnOverride) {
    assert(newPass != 0);

    // Get name here to not do it all over the method.
    const std::string &name = newPass->getName();

    // Do we want to warn the user about squashing a pass?
    if (warnOverride == false) {
      // We use the name as a key.
      std::map<std::string, Pass *>::iterator it = passMap.find(name);

      if (it != passMap.end()) {
        LOG(INFO) << "Pass name " << name << " already used, overwriting pass";
      }
    }

    // Now add to map.
    passMap[name] = newPass;

    // Now add it to the list.
    passList.push_back(newPass);
  }

  void PassDriver::addPassArray(Pass **passes, unsigned int nbr) {
    // Insert each pass into the map and into the list:
    //   - Map is used for the lookup
    //   - List is used for the pass walk
    for (unsigned int i = 0; i < nbr; i++) {
      insertPass(passes[i]);
    }
  }

  void PassDriver::createPasses(void) {
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

    // Add the table into the pass handler.
    addPassArray(passes, nbr);
  }

  void PassDriver::handlePassFlag(CompilationUnit *cUnit, Pass *pass) {
  }

  void PassDriver::dispatchPass(CompilationUnit *cUnit, Pass *curPass) {
    DataflowIterator *iterator = 0;

    LOG(DEBUG) << "Dispatching " << curPass->getName().c_str();

    // Let us start by getting the right iterator.
    DataFlowAnalysisMode mode = curPass->getTraversal();
    MIRGraph *mir_graph = cUnit->mir_graph.get();

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

    // Check the iterator before walking the BasicBlocks.
    assert(iterator != 0);

    bool change = false;
    for (BasicBlock *bb = iterator->Next(change); bb != 0; bb = iterator->Next(change)) {
      change = curPass->walkBasicBlocks(cUnit, bb);
    }

    // We are done with the iterator.
    delete iterator, iterator = 0;
  }

  void PassDriver::applyPass(CompilationUnit *cUnit, Pass *curPass) {
    curPass->start(cUnit);
    dispatchPass(cUnit, curPass);
    curPass->end(cUnit);
  }

  bool PassDriver::runPass(CompilationUnit *cUnit, Pass *curPass, bool timeSplit) {
    // Paranoid: cUnit or curPass cannot be 0, and the pass should have a name.
    if (cUnit == 0 || curPass == 0 || curPass->getName() == "") {
      return false;
    }

    // Do we perform a time split
    if (timeSplit == true) {
      std::string name = "MIROpt:";
      name += curPass->getName();
      cUnit->NewTimingSplit(name.c_str());
    }

    // Check the pass gate first.
    bool shouldApplyPass = curPass->gate(cUnit);

    // If the pass gate said ok.
    if (shouldApplyPass == true) {
      // Applying the pass: first start, doWork, and end calls.
      applyPass(cUnit, curPass);

      // Clean up if need be.
      handlePassFlag(cUnit, curPass);

      // Do we want to log it?
      if ((cUnit->enable_debug & (1 << kDebugDumpCFG)) != 0) {
        // Do we have a pass folder?
        const std::string &passFolder = curPass->getDumpCFGFolder();

        if (passFolder != "") {
          // Create directory prefix.
          std::string prefix = getDumpCFGFolder();
          prefix += passFolder;
          prefix += "/";

          cUnit->mir_graph->DumpCFG(prefix.c_str(), false);
        }
      }
    }

    // If the pass gate passed, we can declare success.
    return shouldApplyPass;
  }

  bool PassDriver::runPass(CompilationUnit *cUnit, const std::string &passName) {
    // Paranoid: cUnit cannot be 0 and we need a pass name.
    if (cUnit == 0 || passName == "") {
      return false;
    }

    Pass *curPass = getPass(passName);

    if (curPass != 0) {
      return runPass(cUnit, curPass);
    }

    // Return false, we did not find the pass.
    return false;
  }

  void PassDriver::launch(void) {
    // Walk the passes.
    for (std::list<Pass *>::iterator it = passList.begin(); it != passList.end(); it++) {
      Pass *curPass = *it;
      runPass(cu, curPass, true);
    }
  }

  void PassDriver::printPassNames(void) const {
    LOG(INFO) << "Loop Passes are:";

    // Walk the passes
    for (std::list<Pass *>::const_iterator it = passList.begin(); it != passList.end(); it++) {
      const Pass *curPass = *it;
      LOG(INFO) << "\t-" << curPass->getName();
    }
  }

  Pass *PassDriver::getPass(const std::string &name) const {
    std::map<std::string, Pass *>::const_iterator it = passMap.find(name);

    // If found, update result.
    if (it != passMap.end()) {
      return it->second;
    }

    return 0;
  }
}  // namespace art

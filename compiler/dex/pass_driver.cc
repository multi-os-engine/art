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

#include "base/logging.h"
#include "base/macros.h"
#include "bb_optimizations.h"
#include "compiler_internals.h"
#include "dataflow_iterator.h"
#include "dataflow_iterator-inl.h"
#include "pass.h"
#include "pass_driver.h"

namespace art {

void (*PassDriver::special_me_pass_driver_selection_)(PassDriver*) = nullptr;

namespace {  // anonymous namespace

/**
 * @brief Helper function to create a single instance of a given Pass and can be shared across
 * the threads.
 */
template <typename PassType>
const Pass* GetPassInstance() {
  static const PassType pass;
  return &pass;
}

void DoWalkBasicBlocks(CompilationUnit* c_unit, const Pass* pass, DataflowIterator* iterator) {
  // Paranoid: Check the iterator before walking the BasicBlocks.
  DCHECK(iterator != nullptr);

  bool change = false;
  for (BasicBlock *bb = iterator->Next(change); bb != 0; bb = iterator->Next(change)) {
    change = pass->WalkBasicBlocks(c_unit, bb);
  }
}

template <typename Iterator>
inline void DoWalkBasicBlocks(CompilationUnit* c_unit, const Pass* pass) {
  Iterator iterator(c_unit->mir_graph.get());
  DoWalkBasicBlocks(c_unit, pass, &iterator);
}

}  // anonymous namespace

PassDriver::PassDriver(CompilationUnit* cu)
    : cu_(cu), dump_cfg_folder_("/sdcard/") {
  DCHECK(cu != nullptr);
  InitializePasses();
}

/*
 * Create the pass list. These passes are immutable and are shared across the threads.
 *
 * Advantage is that there will be no race conditions here.
 * Disadvantage is the passes can't change their internal states depending on CompilationUnit:
 *   - This is not yet an issue: no current pass would require it.
 */
static const Pass* const g_passes[] = {
  GetPassInstance<CacheFieldLoweringInfo>(),
  GetPassInstance<CacheMethodLoweringInfo>(),
  GetPassInstance<CallInlining>(),
  GetPassInstance<CodeLayout>(),
  GetPassInstance<SSATransformation>(),
  GetPassInstance<ConstantPropagation>(),
  GetPassInstance<InitRegLocations>(),
  GetPassInstance<MethodUseCount>(),
  GetPassInstance<NullCheckEliminationAndTypeInference>(),
  GetPassInstance<ClassInitCheckElimination>(),
  GetPassInstance<BBCombine>(),
  GetPassInstance<BBOptimizations>(),
};

// The default pass list is used by CreatePasses to initialize pass_list_.
static std::vector<const Pass*> g_default_pass_list(g_passes, g_passes + arraysize(g_passes));

void PassDriver::CreateDefaultPassList(const std::string& disable_passes) {
  // Insert each pass from g_passes into g_default_pass_list.
  g_default_pass_list.clear();
  g_default_pass_list.reserve(arraysize(g_passes));
  for (const Pass* pass : g_passes) {
    // Check if we should disable this pass.
    if (disable_passes.find(pass->GetName()) != std::string::npos) {
      LOG(INFO) << "Skipping " << pass->GetName();
    } else {
      g_default_pass_list.push_back(pass);
    }
  }
}

void PassDriver::SetDefaultPasses() {
  pass_list_ = g_default_pass_list;
}

void PassDriver::HandlePassFlag(CompilationUnit* c_unit, const Pass* pass) {
  // Unused parameters for the moment.
  UNUSED(c_unit);
  UNUSED(pass);
}

void PassDriver::DispatchPass(CompilationUnit* c_unit, const Pass* curPass) {
  VLOG(compiler) << "Dispatching " << curPass->GetName();

  DataFlowAnalysisMode mode = curPass->GetTraversal();

  switch (mode) {
    case kPreOrderDFSTraversal:
      DoWalkBasicBlocks<PreOrderDfsIterator>(c_unit, curPass);
      break;
    case kRepeatingPreOrderDFSTraversal:
      DoWalkBasicBlocks<RepeatingPreOrderDfsIterator>(c_unit, curPass);
      break;
    case kRepeatingPostOrderDFSTraversal:
      DoWalkBasicBlocks<RepeatingPostOrderDfsIterator>(c_unit, curPass);
      break;
    case kReversePostOrderDFSTraversal:
      DoWalkBasicBlocks<ReversePostOrderDfsIterator>(c_unit, curPass);
      break;
    case kRepeatingReversePostOrderDFSTraversal:
      DoWalkBasicBlocks<RepeatingReversePostOrderDfsIterator>(c_unit, curPass);
      break;
    case kPostOrderDOMTraversal:
      DoWalkBasicBlocks<PostOrderDOMIterator>(c_unit, curPass);
      break;
    case kAllNodes:
      DoWalkBasicBlocks<AllNodesIterator>(c_unit, curPass);
      break;
    case kNoNodes:
      break;
    default:
      LOG(FATAL) << "Iterator mode not handled in dispatcher: " << mode;
      break;
  }
}

void PassDriver::ApplyPass(CompilationUnit* c_unit, const Pass* curPass) {
  curPass->Start(c_unit);
  DispatchPass(c_unit, curPass);
  curPass->End(c_unit);
}

bool PassDriver::RunPass(CompilationUnit* c_unit, const Pass* pass, bool time_split) {
  // Paranoid: c_unit and pass cannot be nullptr, and the pass should have a name.
  DCHECK(c_unit != nullptr);
  DCHECK(pass != nullptr);
  DCHECK(pass->GetName() != nullptr && pass->GetName()[0] != 0);

  // Do we perform a time split
  if (time_split) {
    c_unit->NewTimingSplit(pass->GetName());
  }

  // Check the pass gate first.
  bool should_apply_pass = pass->Gate(c_unit);

  if (should_apply_pass) {
    // Applying the pass: first start, doWork, and end calls.
    ApplyPass(c_unit, pass);

    // Clean up if need be.
    HandlePassFlag(c_unit, pass);

    // Do we want to log it?
    if ((c_unit->enable_debug&  (1 << kDebugDumpCFG)) != 0) {
      // Do we have a pass folder?
      const char* passFolder = pass->GetDumpCFGFolder();
      DCHECK(passFolder != nullptr);

      if (passFolder[0] != 0) {
        // Create directory prefix.
        std::string prefix = GetDumpCFGFolder();
        prefix += passFolder;
        prefix += "/";

        c_unit->mir_graph->DumpCFG(prefix.c_str(), false);
      }
    }
  }

  // If the pass gate passed, we can declare success.
  return should_apply_pass;
}

bool PassDriver::RunPass(CompilationUnit* c_unit, const char* pass_name) {
  // Paranoid: c_unit cannot be nullptr and we need a pass name.
  DCHECK(c_unit != nullptr);
  DCHECK(pass_name != nullptr && pass_name[0] != 0);

  const Pass* cur_pass = GetPass(pass_name);

  if (cur_pass != nullptr) {
    return RunPass(c_unit, cur_pass);
  }

  // Return false, we did not find the pass.
  return false;
}

void PassDriver::Launch() {
  for (const Pass* cur_pass : pass_list_) {
    RunPass(cu_, cur_pass, true);
  }
}

void PassDriver::PrintPassNames() {
  LOG(INFO) << "Loop Passes are:";

  for (const Pass* cur_pass : g_default_pass_list) {
    LOG(INFO) << "\t-" << cur_pass->GetName();
  }
}

const Pass* PassDriver::GetPass(const char* name) const {
  for (const Pass* cur_pass : pass_list_) {
    if (strcmp(name, cur_pass->GetName()) == 0) {
      return cur_pass;
    }
  }
  return nullptr;
}

bool PassDriver::HandleUserPass(Pass* pass, const char* name, PassInstrumentation mode) {
  // Walk the pass list and find the pass.
  const Pass* cur_pass = nullptr;
  unsigned int idx = 0;
  unsigned int size = g_default_pass_list.size();

  // idx is not defined in the loop because we will need it below.
  for (idx = 0; idx < size; idx++) {
    cur_pass = g_default_pass_list[idx];
    if (strcmp(name, cur_pass->GetName()) == 0) {
      break;
    }
  }

  // Paranoid: didn't find the name.
  if (idx == size) {
    LOG(INFO) << "Pass Modification could not find the reference pass name, here is what you provided: " << name;
    LOG(INFO) << "\t- Here are the loop passes for reference:";
    PrintPassNames();
    return false;
  }

  // We have the pass reference, what we do now depends on the mode.
  // We have a bit of work here sometimes because the list is in vector form.
  // We are reusing idx here, it represents the index of the pass that has "name" as its name.
  switch (mode) {
    case kPassReplace:
      g_default_pass_list[idx] = pass;
      break;
    case kPassInsertBefore:
      g_default_pass_list.push_back(nullptr);

      // We know we found it so size > 0, thus this is fine.
      // Note we start at size because we pushed back something right before.
      for (unsigned int i = size; i > idx; i--) {
        // Same reason makes the -1 here safe.
        g_default_pass_list[i] = g_default_pass_list[i - 1];
      }
      g_default_pass_list[idx] = pass;
      break;
    case kPassInsertAfter:
      g_default_pass_list.push_back(nullptr);

      // We know we found it so size > 0, thus this is fine.
      // Note we start at size because we pushed back something right before.
      unsigned int i;
      for (i = size; i > idx + 1; i--) {
        // Same reason makes the -1 here safe.
        g_default_pass_list[i] = g_default_pass_list[i - 1];
      }
      g_default_pass_list[i] = pass;
      break;
    case kPassRemove:
      // We know we found it so size > 0, thus this is fine.
      // Note we start at size because we pushed back something right before.
      for (unsigned i = idx; i < size - 1; i++) {
        g_default_pass_list[i] = g_default_pass_list[i + 1];
      }
      // We can now remove the last one.
      g_default_pass_list.pop_back();
      break;
    default:
      break;
  }

  // Report success
  return true;
}

void PassDriver::CopyPasses(std::vector<const Pass*> &passes) {
  pass_list_ = passes;
}

}  // namespace art

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

#ifndef ART_COMPILER_DEX_PASS_DRIVER_ME_H_
#define ART_COMPILER_DEX_PASS_DRIVER_ME_H_

#include <cstdlib>
#include <cstring>

#include "bb_optimizations.h"
#include "dataflow_iterator.h"
#include "dataflow_iterator-inl.h"
#include "dex_flags.h"
#include "pass_driver.h"
#include "pass_me.h"
#include "safe_map.h"


namespace art {

class PassManager;
class PassManagerOptions;

class PassDriverME: public PassDriver {
 public:
  explicit PassDriverME(const PassManager* const pass_manager, CompilationUnit* cu)
      : PassDriver(pass_manager), pass_me_data_holder_(), dump_cfg_folder_("/sdcard/") {
        pass_me_data_holder_.bb = nullptr;
        pass_me_data_holder_.c_unit = cu;
  }

  ~PassDriverME() {
  }

  void DispatchPass(const Pass* pass) {
    VLOG(compiler) << "Dispatching " << pass->GetName();
    const PassME* me_pass = down_cast<const PassME*>(pass);

    DataFlowAnalysisMode mode = me_pass->GetTraversal();

    switch (mode) {
      case kPreOrderDFSTraversal:
        DoWalkBasicBlocks<PreOrderDfsIterator>(&pass_me_data_holder_, me_pass);
        break;
      case kRepeatingPreOrderDFSTraversal:
        DoWalkBasicBlocks<RepeatingPreOrderDfsIterator>(&pass_me_data_holder_, me_pass);
        break;
      case kRepeatingPostOrderDFSTraversal:
        DoWalkBasicBlocks<RepeatingPostOrderDfsIterator>(&pass_me_data_holder_, me_pass);
        break;
      case kReversePostOrderDFSTraversal:
        DoWalkBasicBlocks<ReversePostOrderDfsIterator>(&pass_me_data_holder_, me_pass);
        break;
      case kRepeatingReversePostOrderDFSTraversal:
        DoWalkBasicBlocks<RepeatingReversePostOrderDfsIterator>(&pass_me_data_holder_, me_pass);
        break;
      case kPostOrderDOMTraversal:
        DoWalkBasicBlocks<PostOrderDOMIterator>(&pass_me_data_holder_, me_pass);
        break;
      case kTopologicalSortTraversal:
        DoWalkBasicBlocks<TopologicalSortIterator>(&pass_me_data_holder_, me_pass);
        break;
      case kLoopRepeatingTopologicalSortTraversal:
        DoWalkBasicBlocks<LoopRepeatingTopologicalSortIterator>(&pass_me_data_holder_, me_pass);
        break;
      case kAllNodes:
        DoWalkBasicBlocks<AllNodesIterator>(&pass_me_data_holder_, me_pass);
        break;
      case kNoNodes:
        break;
      default:
        LOG(FATAL) << "Iterator mode not handled in dispatcher: " << mode;
        break;
    }
  }

  bool RunPass(const Pass* pass, bool time_split) OVERRIDE;

  static void PrintPassOptions(PassManager* manager);

  const char* GetDumpCFGFolder() const {
    return dump_cfg_folder_;
  }

 protected:
  /** @brief The data holder that contains data needed for the PassDriverME. */
  PassMEDataHolder pass_me_data_holder_;

  /** @brief Dump CFG base folder: where is the base folder for dumping CFGs. */
  const char* dump_cfg_folder_;

  static void DoWalkBasicBlocks(PassMEDataHolder* data, const PassME* pass,
                                DataflowIterator* iterator) {
    // Paranoid: Check the iterator before walking the BasicBlocks.
    DCHECK(iterator != nullptr);
    bool change = false;
    for (BasicBlock* bb = iterator->Next(change); bb != nullptr; bb = iterator->Next(change)) {
      data->bb = bb;
      change = pass->Worker(data);
    }
  }

  template <typename Iterator>
  inline static void DoWalkBasicBlocks(PassMEDataHolder* data, const PassME* pass) {
      DCHECK(data != nullptr);
      CompilationUnit* c_unit = data->c_unit;
      DCHECK(c_unit != nullptr);
      Iterator iterator(c_unit->mir_graph.get());
      DoWalkBasicBlocks(data, pass, &iterator);
    }

  /**
   * @brief Fills the settings_to_fill by finding all of the applicable options in the
   * overridden_pass_options_list_.
   * @param pass_name The pass name for which to fill settings.
   * @param settings_to_fill Fills the options to contain the mapping of name of option to the new
   * configuration.
   */
  static void FillOverriddenPassSettings(const PassManagerOptions* options, const char* pass_name,
                                         SafeMap<const std::string, int>& settings_to_fill);
};
}  // namespace art
#endif  // ART_COMPILER_DEX_PASS_DRIVER_ME_H_


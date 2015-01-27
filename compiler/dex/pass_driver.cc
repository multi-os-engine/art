/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "pass_driver.h"

#include "pass.h"
#include "pass_manager.h"

namespace art {

PassDriver::PassDriver(const PassManager* const pass_manager) : pass_manager_(pass_manager) {
  pass_list_ = *pass_manager_->GetDefaultPassList();
  DCHECK(!pass_list_.empty());
}

void PassDriver::InsertPass(const Pass* new_pass) {
  DCHECK(new_pass != nullptr);
  DCHECK(new_pass->GetName() != nullptr);
  DCHECK_NE(new_pass->GetName()[0], 0);
  // It is an error to override an existing pass.
  DCHECK(GetPass(new_pass->GetName()) == nullptr)
      << "Pass name " << new_pass->GetName() << " already used.";
  // Now add to the list.
  pass_list_.push_back(new_pass);
}

bool PassDriver::RunPass(const char* pass_name) {
  // Paranoid: c_unit cannot be nullptr and we need a pass name.
  DCHECK(pass_name != nullptr);
  DCHECK_NE(pass_name[0], 0);
  const Pass* cur_pass = GetPass(pass_name);
  if (cur_pass != nullptr) {
    return RunPass(cur_pass);
  }
  // Return false, we did not find the pass.
  return false;
}

const Pass* PassDriver::GetPass(const char* name) const {
  for (const Pass* cur_pass : pass_list_) {
    if (strcmp(name, cur_pass->GetName()) == 0) {
      return cur_pass;
    }
  }
  return nullptr;
}

void PassDriver::ApplyPass(PassDataHolder* data, const Pass* pass) {
  pass->Start(data);
  DispatchPass(pass);
  pass->End(data);
}

}  // namespace art

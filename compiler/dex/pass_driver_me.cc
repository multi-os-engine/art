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

#include "pass_driver_me.h"

#include "pass.h"
#include "pass_manager.h"

namespace art {
  bool PassDriverME::RunPass(const Pass* pass, bool time_split) {
    // Paranoid: c_unit and pass cannot be nullptr, and the pass should have a name
    DCHECK(pass != nullptr);
    DCHECK(pass->GetName() != nullptr && pass->GetName()[0] != 0);
    CompilationUnit* c_unit = pass_me_data_holder_.c_unit;
    DCHECK(c_unit != nullptr);

    // Do we perform a time split
    if (time_split) {
      c_unit->NewTimingSplit(pass->GetName());
    }

    // First, work on determining pass verbosity.
    bool old_print_pass = c_unit->print_pass;
    c_unit->print_pass = pass_manager_->GetOptions().GetPrintAllPasses();
    auto* const options = &pass_manager_->GetOptions();
    const std::string& print_pass_list = options->GetPrintPassList();
    if (!print_pass_list.empty() && strstr(print_pass_list.c_str(), pass->GetName()) != nullptr) {
      c_unit->print_pass = true;
    }

    // Next, check if there are any overridden settings for the pass that change default configuration.
    c_unit->overridden_pass_options.clear();
    FillOverriddenPassSettings(options, pass->GetName(), c_unit->overridden_pass_options);
    if (c_unit->print_pass) {
      for (auto setting_it : c_unit->overridden_pass_options) {
        LOG(INFO) << "Overridden option \"" << setting_it.first << ":"
          << setting_it.second << "\" for pass \"" << pass->GetName() << "\"";
      }
    }

    // Check the pass gate first.
    bool should_apply_pass = pass->Gate(&pass_me_data_holder_);
    if (should_apply_pass) {
      // Applying the pass: first start, doWork, and end calls.
      this->ApplyPass(&pass_me_data_holder_, pass);

      bool should_dump = (c_unit->enable_debug & (1 << kDebugDumpCFG)) != 0;

      const std::string& dump_pass_list = pass_manager_->GetOptions().GetDumpPassList();
      if (!dump_pass_list.empty()) {
        const bool found = strstr(dump_pass_list.c_str(), pass->GetName());
        should_dump = should_dump || found;
      }

      if (should_dump) {
        // Do we want to log it?
        if ((c_unit->enable_debug&  (1 << kDebugDumpCFG)) != 0) {
          // Do we have a pass folder?
          const PassME* me_pass = (down_cast<const PassME*>(pass));
          const char* passFolder = me_pass->GetDumpCFGFolder();
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
    }

    // Before wrapping up with this pass, restore old pass verbosity flag.
    c_unit->print_pass = old_print_pass;

    // If the pass gate passed, we can declare success.
    return should_apply_pass;
  }

  void PassDriverME::PrintPassOptions(PassManager* manager) {
    for (const auto* pass : *manager->GetDefaultPassList()) {
      const PassME* me_pass = down_cast<const PassME*>(pass);
      if (me_pass->HasOptions()) {
        LOG(INFO) << "Pass options for \"" << me_pass->GetName() << "\" are:";
        SafeMap<const std::string, int> overridden_settings;
        FillOverriddenPassSettings(&manager->GetOptions(), me_pass->GetName(),
                                   overridden_settings);
        me_pass->PrintPassOptions(overridden_settings);
      }
    }
  }

  void PassDriverME::FillOverriddenPassSettings(
      const PassManagerOptions* options, const char* pass_name,
      SafeMap<const std::string, int>& settings_to_fill) {
    const std::string& settings = options->GetOverriddenPassOptions();
    const size_t settings_len = settings.size();

    // Before anything, check if we care about anything right now.
    if (settings_len == 0) {
      return;
    }

    const size_t pass_name_len = strlen(pass_name);
    const size_t min_setting_size = 4;  // 2 delimiters, 1 setting name, 1 setting
    size_t search_pos = 0;

    // If there is no room for pass options, exit early.
    if (settings_len < pass_name_len + min_setting_size) {
      return;
    }

    do {
      search_pos = settings.find(pass_name, search_pos);

      // Check if we found this pass name in rest of string.
      if (search_pos == std::string::npos) {
        // No more settings for this pass.
        break;
      }

      // The string contains the pass name. Now check that there is
      // room for the settings: at least one char for setting name,
      // two chars for two delimiter, and at least one char for setting.
      if (search_pos + pass_name_len + min_setting_size >= settings_len) {
        // No more settings for this pass.
        break;
      }

      // Update the current search position to not include the pass name.
      search_pos += pass_name_len;

      // The format must be "PassName:SettingName:#" where # is the setting.
      // Thus look for the first ":" which must exist.
      if (settings[search_pos] != ':') {
        // Missing delimiter right after pass name.
        continue;
      } else {
        search_pos += 1;
      }

      // Now look for the actual setting by finding the next ":" delimiter.
      const size_t setting_name_pos = search_pos;
      size_t setting_pos = settings.find(':', setting_name_pos);

      if (setting_pos == std::string::npos) {
        // Missing a delimiter that would capture where setting starts.
        continue;
      } else if (setting_pos == setting_name_pos) {
        // Missing setting thus did not move from setting name
        continue;
      } else {
        // Skip the delimiter.
        setting_pos += 1;
      }

      // Look for the terminating delimiter which must be a comma.
      size_t next_configuration_separator = settings.find(',', setting_pos);
      if (next_configuration_separator == std::string::npos) {
        next_configuration_separator = settings_len;
      }

      // Prevent end of string errors.
      if (next_configuration_separator == setting_pos) {
          continue;
      }

      // Get the actual setting itself. Strtol is being used to convert because it is
      // exception safe. If the input is not sane, it will set a setting of 0.
      std::string setting_string =
          settings.substr(setting_pos, next_configuration_separator - setting_pos);
      int setting = std::strtol(setting_string.c_str(), 0, 0);

      std::string setting_name =
          settings.substr(setting_name_pos, setting_pos - setting_name_pos - 1);

      settings_to_fill.Put(setting_name, setting);

      search_pos = next_configuration_separator;
    } while (true);
  }
}  // namespace art

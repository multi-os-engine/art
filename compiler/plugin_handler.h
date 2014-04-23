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

#ifndef ART_COMPILER_PLUGIN_HANDLER_H_
#define ART_COMPILER_PLUGIN_HANDLER_H_

#include <string>
#include <vector>
#include <map>

namespace art {

/*
 * Note: For each file in the directory (/system/lib/plugins currently), attempt to load the file as a shared lib.
 *       Initialize it by calling its ArtPluginInit() function.
 *       It's up to this function to connect the objects in the plugin, whatever they are, into ART.
 *       For instance, if the plugin shared library contains a Pass object, the init routine would probably want to call into art::PassDriver::HandleUserPass
 *
 *       Currently there is no check on compatibility between the plugins and general libart.
 *       This should be done in a hand-shake manner where both libart and the plugin check if there are incompatibility issues.
 */
void LoadUpPlugins(const char* directory);

}  // namespace art
#endif  // ART_COMPILER_PLUGIN_HANDLER_H_

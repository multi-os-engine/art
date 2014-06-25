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

#ifndef VTUNE_SUPPORT_H_
#define VTUNE_SUPPORT_H_

#include "oat_file.h"

namespace art {

/*
 * @brief Prepare data about all methods in the oat file and send it to VTune.
 * @param oat_file
 */
void SendOatFileToVTune(OatFile &oat_file);

}  // namespace art

#endif // VTUNE_SUPPORT_H_

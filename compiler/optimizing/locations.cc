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
#include "base/stringprintf.h"
#include "locations.h"
#include "nodes.h"

namespace art {

const char* Location::DebugString() const {
  const char* type = "?";
  switch (GetKind()) {
    case kInvalid: type = "?"; break;
    case kRegister: type = "R"; break;
    case kStackSlot: type = "S"; break;
    case kDoubleStackSlot: type = "DS"; break;
    case kQuickParameter: type = "Q"; break;
    case kUnallocated: type = "U"; break;
  }
  return StringPrintf("%s%d", type, static_cast<int>(GetPayload())).c_str();
}

LocationSummary::LocationSummary(HInstruction* instruction)
    : inputs_(instruction->GetBlock()->GetGraph()->GetArena(), instruction->InputCount()),
      temps_(instruction->GetBlock()->GetGraph()->GetArena(), 0) {
  inputs_.SetSize(instruction->InputCount());
  for (size_t i = 0; i < instruction->InputCount(); i++) {
    inputs_.Put(i, Location());
  }
}

std::string LocationSummary::DebugString() const {
  std::string result;
  bool comma = false;
  result += " I(";
  for (size_t i = 0, e = GetInputCount(); i < e; ++i) {
    Location loc = InAt(i);
    if (comma) {
      result += ",";
    }
    result += loc.DebugString();
    comma = true;
  }
  result += ") T(";
  comma = false;
  for (size_t i = 0, e = GetTempCount(); i < e; ++i) {
    Location loc = GetTemp(i);
    if (comma) {
      result += ",";
    }
    result += loc.DebugString();
    comma = true;
  }
  result += "): ";
  result += output_.DebugString();
  return result;
}


}  // namespace art

# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

class FileSplitter(object):
  """ TODO """

  def _parseStream(self, stream):
    lineNo = 0
    allGroups = []
    currentGroup = None

    for line in stream:
      lineNo += 1
      line = line.strip()
      if not line:
        continue

      # Let the child class process the line and return information about it.
      # The _processLine method can modify the content of the line (or delete it
      # entirely) and specify whether it starts a new group.
      processedLine, newGroupName = self._processLine(line, lineNo)
      if newGroupName is not None:
        currentGroup = (newGroupName, [], lineNo)
        allGroups.append(currentGroup)
      if processedLine is not None:
        if currentGroup is not None:
          currentGroup[1].append(processedLine)
        else:
          self._exceptionLineOutsideGroup(line, lineNo)

    # Finally, take the generated line groups and let the child class process
    # each one before storing the final outcome.
    map(lambda group: self._processGroup(group[0], group[1], group[2]), allGroups)

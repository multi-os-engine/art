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

from common.logger                   import Logger
from file_format.file_splitter       import FileSplitter
from file_format.c1visualizer.struct import C1visualizerFile, C1visualizerPass

import re

class C1visualizerParser(FileSplitter):
  class ParsingState:
    OutsideBlock, InsideCompilationBlock, StartingCfgBlock, InsideCfgBlock = range(4)

  # This function is invoked on each line of the output file and returns a pair
  # which instructs the parser how the line should be handled. If the line is to
  # be included in the current group, it is returned in the first value. If the
  # line starts a new output group, the name of the group is returned in the
  # second value.
  def _processLine(self, line, lineNo):
    if self.state == C1visualizerParser.ParsingState.StartingCfgBlock:
      # Previous line started a new 'cfg' block which means that this one must
      # contain the name of the pass (this is enforced by C1visualizer).
      if re.match("name\s+\"[^\"]+\"", line):
        # Extract the pass name, prepend it with the name of the method and
        # return as the beginning of a new group.
        self.state = C1visualizerParser.ParsingState.InsideCfgBlock
        return (None, self.lastMethodName + " " + line.split("\"")[1])
      else:
        Logger.fail("Expected output group name", self.fileName, lineNo)

    elif self.state == C1visualizerParser.ParsingState.InsideCfgBlock:
      if line == "end_cfg":
        self.state = C1visualizerParser.ParsingState.OutsideBlock
        return (None, None)
      else:
        return (line, None)

    elif self.state == C1visualizerParser.ParsingState.InsideCompilationBlock:
      # Search for the method's name. Format: method "<name>"
      if re.match("method\s+\"[^\"]*\"", line):
        methodName = line.split("\"")[1].strip()
        if not methodName:
          Logger.fail("Empty method name in output", self.fileName, lineNo)
        self.lastMethodName = methodName
      elif line == "end_compilation":
        self.state = C1visualizerParser.ParsingState.OutsideBlock
      return (None, None)

    else:
      assert self.state == C1visualizerParser.ParsingState.OutsideBlock
      if line == "begin_cfg":
        # The line starts a new group but we'll wait until the next line from
        # which we can extract the name of the pass.
        if self.lastMethodName is None:
          Logger.fail("Expected method header", self.fileName, lineNo)
        self.state = C1visualizerParser.ParsingState.StartingCfgBlock
        return (None, None)
      elif line == "begin_compilation":
        self.state = C1visualizerParser.ParsingState.InsideCompilationBlock
        return (None, None)
      else:
        Logger.fail("C1visualizer line not inside a group", self.fileName, lineNo)

  # Constructs an output group from the parser-collected output lines.
  def _processGroup(self, name, lines, lineNo):
    return C1visualizerPass(self.c1File, name, lines, lineNo + 1)

  def parse(self, fileName, stream):
    self.fileName = fileName
    self.lastMethodName = None
    self.state = C1visualizerParser.ParsingState.OutsideBlock
    self.c1File = C1visualizerFile(fileName)
    self._parseStream(stream)
    return self.c1File

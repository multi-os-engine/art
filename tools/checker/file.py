#!/usr/bin/env python2
#
# Copyright (C) 2014 The Android Open Source Project
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

from group  import CheckGroup, OutputGroup
from line   import CheckLine
from logger import Logger

import re

class FileSplitMixin(object):
  """Mixin for representing text files which need to be split into smaller
     chunks before being parsed."""

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
    return list(map(lambda group: self._processGroup(group[0], group[1], group[2]), allGroups))


class CheckFile(FileSplitMixin):
  """Collection of check groups extracted from the input test file."""

  def __init__(self, prefix, checkStream, fileName=None):
    self.fileName = fileName
    self.prefix = prefix
    self.groups = self._parseStream(checkStream)

  # Attempts to parse a check line. The regex searches for a comment symbol
  # followed by the CHECK keyword, given attribute and a colon at the very
  # beginning of the line. Whitespaces are ignored.
  def _extractLine(self, prefix, line):
    rIgnoreWhitespace = r"\s*"
    rCommentSymbols = [r"//", r"#"]
    regexPrefix = rIgnoreWhitespace + \
                  r"(" + r"|".join(rCommentSymbols) + r")" + \
                  rIgnoreWhitespace + \
                  prefix + r":"

    # The 'match' function succeeds only if the pattern is matched at the
    # beginning of the line.
    match = re.match(regexPrefix, line)
    if match is not None:
      return line[match.end():].strip()
    else:
      return None

  # This function is invoked on each line of the check file and returns a pair
  # which instructs the parser how the line should be handled. If the line is to
  # be included in the current check group, it is returned in the first value.
  # If the line starts a new check group, the name of the group is returned in
  # the second value.
  def _processLine(self, line, lineNo):
    # Lines beginning with 'CHECK-START' start a new check group.
    startLine = self._extractLine(self.prefix + "-START", line)
    if startLine is not None:
      return None, startLine

    # Lines starting only with 'CHECK' are matched in order.
    plainLine = self._extractLine(self.prefix, line)
    if plainLine is not None:
      return (plainLine, CheckLine.Variant.InOrder, lineNo), None

    # 'CHECK-DAG' lines are no-order assertions.
    dagLine = self._extractLine(self.prefix + "-DAG", line)
    if dagLine is not None:
      return (dagLine, CheckLine.Variant.DAG, lineNo), None

    # 'CHECK-NOT' lines are no-order negative assertions.
    notLine = self._extractLine(self.prefix + "-NOT", line)
    if notLine is not None:
      return (notLine, CheckLine.Variant.Not, lineNo), None

    # Other lines are ignored.
    return None, None

  def _exceptionLineOutsideGroup(self, line, lineNo):
    Logger.fail("Check line not inside a group", self.fileName, lineNo)

  # Constructs a check group from the parser-collected check lines.
  def _processGroup(self, name, lines, lineNo):
    checkLines = list(map(lambda line: CheckLine(line[0], line[1], self.fileName, line[2]), lines))
    return CheckGroup(name, checkLines, self.fileName, lineNo)

  def match(self, outputFile):
    for checkGroup in self.groups:
      # TODO: Currently does not handle multiple occurrences of the same group
      # name, e.g. when a pass is run multiple times. It will always try to
      # match a check group against the first output group of the same name.
      outputGroup = outputFile.findGroup(checkGroup.name)
      if outputGroup is None:
        Logger.fail("Group \"" + checkGroup.name + "\" not found in the output",
                    self.fileName, checkGroup.lineNo)
      Logger.startTest(checkGroup.name)
      checkGroup.match(outputGroup)
      Logger.testPassed()


class OutputFile(FileSplitMixin):
  """Representation of the output generated by the test and split into groups
     within which the checks are performed.

     C1visualizer format is parsed with a state machine which differentiates
     between the 'compilation' and 'cfg' blocks. The former marks the beginning
     of a method. It is parsed for the method's name but otherwise ignored. Each
     subsequent CFG block represents one stage of the compilation pipeline and
     is parsed into an output group named "<method name> <pass name>".
     """

  class ParsingState:
    OutsideBlock, InsideCompilationBlock, StartingCfgBlock, InsideCfgBlock = range(4)

  def __init__(self, outputStream, fileName=None):
    self.fileName = fileName

    # Initialize the state machine
    self.lastMethodName = None
    self.state = OutputFile.ParsingState.OutsideBlock
    self.groups = self._parseStream(outputStream)

  # This function is invoked on each line of the output file and returns a pair
  # which instructs the parser how the line should be handled. If the line is to
  # be included in the current group, it is returned in the first value. If the
  # line starts a new output group, the name of the group is returned in the
  # second value.
  def _processLine(self, line, lineNo):
    if self.state == OutputFile.ParsingState.StartingCfgBlock:
      # Previous line started a new 'cfg' block which means that this one must
      # contain the name of the pass (this is enforced by C1visualizer).
      if re.match("name\s+\"[^\"]+\"", line):
        # Extract the pass name, prepend it with the name of the method and
        # return as the beginning of a new group.
        self.state = OutputFile.ParsingState.InsideCfgBlock
        return (None, self.lastMethodName + " " + line.split("\"")[1])
      else:
        Logger.fail("Expected output group name", self.fileName, lineNo)

    elif self.state == OutputFile.ParsingState.InsideCfgBlock:
      if line == "end_cfg":
        self.state = OutputFile.ParsingState.OutsideBlock
        return (None, None)
      else:
        return (line, None)

    elif self.state == OutputFile.ParsingState.InsideCompilationBlock:
      # Search for the method's name. Format: method "<name>"
      if re.match("method\s+\"[^\"]*\"", line):
        methodName = line.split("\"")[1].strip()
        if not methodName:
          Logger.fail("Empty method name in output", self.fileName, lineNo)
        self.lastMethodName = methodName
      elif line == "end_compilation":
        self.state = OutputFile.ParsingState.OutsideBlock
      return (None, None)

    else:
      assert self.state == OutputFile.ParsingState.OutsideBlock
      if line == "begin_cfg":
        # The line starts a new group but we'll wait until the next line from
        # which we can extract the name of the pass.
        if self.lastMethodName is None:
          Logger.fail("Expected method header", self.fileName, lineNo)
        self.state = OutputFile.ParsingState.StartingCfgBlock
        return (None, None)
      elif line == "begin_compilation":
        self.state = OutputFile.ParsingState.InsideCompilationBlock
        return (None, None)
      else:
        Logger.fail("Output line not inside a group", self.fileName, lineNo)

  # Constructs an output group from the parser-collected output lines.
  def _processGroup(self, name, lines, lineNo):
    return OutputGroup(name, lines, self.fileName, lineNo + 1)

  def findGroup(self, name):
    for group in self.groups:
      if group.name == name:
        return group
    return None

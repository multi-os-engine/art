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

from common import EqualityMixin
from line   import CheckLine
from logger import Logger

class CheckGroup(EqualityMixin):
  """Represents a named collection of check lines which are to be matched
     against an output group of the same name."""

  def __init__(self, name, lines, fileName=None, lineNo=-1):
    self.fileName = fileName
    self.lineNo = lineNo

    if not name:
      Logger.fail("Check group does not have a name", self.fileName, self.lineNo)
    if not lines:
      Logger.fail("Check group does not have a body", self.fileName, self.lineNo)

    self.name = name
    self.lines = lines

  def __eq__(self, other):
    return (isinstance(other, self.__class__) and
            self.name == other.name and
            self.lines == other.lines)

  def __headAndTail(self, list):
    return list[0], list[1:]

  # Splits a list of check lines at index 'i' such that lines[i] is the first
  # element whose variant is not equal to the given parameter.
  def __splitByVariant(self, lines, variant):
    i = 0
    while i < len(lines) and lines[i].variant == variant:
      i += 1
    return lines[:i], lines[i:]

  # Extracts the first sequence of check lines which are independent of each
  # other's match location, i.e. either consecutive DAG lines or a single
  # InOrder line. Any Not lines preceeding this sequence are also extracted.
  def __nextIndependentChecks(self, checkLines):
    notChecks, checkLines = self.__splitByVariant(checkLines, CheckLine.Variant.Not)
    if not checkLines:
      return notChecks, [], []

    head, tail = self.__headAndTail(checkLines)
    if head.variant == CheckLine.Variant.InOrder:
      return notChecks, [head], tail
    else:
      assert head.variant == CheckLine.Variant.DAG
      independentChecks, checkLines = self.__splitByVariant(checkLines, CheckLine.Variant.DAG)
      return notChecks, independentChecks, checkLines

  # If successful, returns the line number of the first output line matching the
  # check line and the updated variable state. Otherwise returns -1 and None,
  # respectively. The 'lineFilter' parameter can be used to supply a list of
  # line numbers (counting from 1) which should be skipped.
  def __findFirstMatch(self, checkLine, outputLines, startLineNo, lineFilter, varState):
    matchLineNo = startLineNo
    for outputLine in outputLines:
      if matchLineNo not in lineFilter:
        newVarState = checkLine.match(outputLine, varState)
        if newVarState is not None:
          return matchLineNo, newVarState
      matchLineNo += 1
    return -1, None

  # Matches the given positive check lines against the output in order of
  # appearance. Variable state is propagated but the scope of the search remains
  # the same for all checks. Each output line can only be matched once.
  # If all check lines are matched, the resulting variable state is returned
  # together with the remaining output. The function also returns output lines
  # which appear before either of the matched lines so they can be tested
  # against Not checks.
  def __matchIndependentChecks(self, checkLines, outputLines, startLineNo, varState):
    # If no checks are provided, skip over the entire output.
    if not checkLines:
      return outputLines, [], startLineNo + len(outputLines), varState

    # Keep track of which lines have been matched.
    matchedLines = []

    # Find first unused output line which matches each check line.
    for checkLine in checkLines:
      matchLineNo, varState = \
        self.__findFirstMatch(checkLine, outputLines, startLineNo, matchedLines, varState)
      if varState is None:
        Logger.testFailed("Could not match check line \"" + checkLine.content + "\" " +
                          "starting from output line " + str(startLineNo),
                          self.fileName, checkLine.lineNo)
      matchedLines.append(matchLineNo)

    # Return new variable state and the output lines which lie outside the
    # match locations of this independent group.
    minMatchLineNo = min(matchedLines)
    maxMatchLineNo = max(matchedLines)
    preceedingLines = outputLines[:minMatchLineNo - startLineNo]
    remainingLines = outputLines[maxMatchLineNo - startLineNo + 1:]
    return preceedingLines, remainingLines, maxMatchLineNo + 1, varState

  # Makes sure that the given check lines do not match any of the given output
  # lines. Variable state does not change.
  def __matchNotLines(self, checkLines, outputLines, startLineNo, varState):
    for checkLine in checkLines:
      assert checkLine.variant == CheckLine.Variant.Not
      matchLineNo, matchVarState = \
        self.__findFirstMatch(checkLine, outputLines, startLineNo, [], varState)
      if matchVarState is not None:
        Logger.testFailed("CHECK-NOT line \"" + checkLine.content + "\" matches output line " + \
                          str(matchLineNo), self.fileName, checkLine.lineNo)

  # Matches the check lines in this group against an output group. It is
  # responsible for running the checks in the right order and scope, and
  # for propagating the variable state between the check lines.
  def match(self, outputGroup):
    varState = {}
    checkLines = self.lines
    outputLines = outputGroup.body
    startLineNo = outputGroup.lineNo

    while checkLines:
      # Extract the next sequence of location-independent checks to be matched.
      notChecks, independentChecks, checkLines = self.__nextIndependentChecks(checkLines)

      # Match the independent checks.
      notOutput, outputLines, newStartLineNo, newVarState = \
        self.__matchIndependentChecks(independentChecks, outputLines, startLineNo, varState)

      # Run the Not checks against the output lines which lie between the last
      # two independent groups or the bounds of the output.
      self.__matchNotLines(notChecks, notOutput, startLineNo, varState)

      # Update variable state.
      startLineNo = newStartLineNo
      varState = newVarState

class OutputGroup(EqualityMixin):
  """Represents a named part of the test output against which a check group of
     the same name is to be matched."""

  def __init__(self, name, body, fileName=None, lineNo=-1):
    if not name:
      Logger.fail("Output group does not have a name", fileName, lineNo)
    if not body:
      Logger.fail("Output group does not have a body", fileName, lineNo)

    self.name = name
    self.body = body
    self.lineNo = lineNo

  def __eq__(self, other):
    return (isinstance(other, self.__class__) and
            self.name == other.name and
            self.body == other.body)

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

from logger import Logger

import common
import re

class CheckElement(common.EqualityMixin):
  """Single element of the check line."""

  class Variant(object):
    """Supported language constructs."""
    Text, Pattern, VarRef, VarDef, Separator = range(5)

  rStartOptional = r"("
  rEndOptional = r")?"

  rName = r"([a-zA-Z][a-zA-Z0-9]*)"
  rRegex = r"(.+?)"
  rPatternStartSym = r"(\{\{)"
  rPatternEndSym = r"(\}\})"
  rVariableStartSym = r"(\[\[)"
  rVariableEndSym = r"(\]\])"
  rVariableSeparator = r"(:)"

  regexPattern = rPatternStartSym + rRegex + rPatternEndSym
  regexVariable = rVariableStartSym + \
                    rName + \
                    (rStartOptional + rVariableSeparator + rRegex + rEndOptional) + \
                  rVariableEndSym

  def __init__(self, variant, name, pattern):
    self.variant = variant
    self.name = name
    self.pattern = pattern

  @staticmethod
  def newSeparator():
    return CheckElement(CheckElement.Variant.Separator, None, None)

  @staticmethod
  def parseText(text):
    return CheckElement(CheckElement.Variant.Text, None, re.escape(text))

  @staticmethod
  def parsePattern(patternElem):
    return CheckElement(CheckElement.Variant.Pattern, None, patternElem[2:-2])

  @staticmethod
  def parseVariable(varElem):
    colonPos = varElem.find(":")
    if colonPos == -1:
      # Variable reference
      name = varElem[2:-2]
      return CheckElement(CheckElement.Variant.VarRef, name, None)
    else:
      # Variable definition
      name = varElem[2:colonPos]
      body = varElem[colonPos+1:-2]
      return CheckElement(CheckElement.Variant.VarDef, name, body)

class CheckLine(common.EqualityMixin):
  """Representation of a single assertion in the check file formed of one or
     more regex elements. Matching against an output line is successful only
     if all regex elements can be matched in the given order."""

  class Variant(object):
    """Supported types of assertions."""
    InOrder, DAG, Not = range(3)

  def __init__(self, content, variant=Variant.InOrder, fileName=None, lineNo=-1):
    self.fileName = fileName
    self.lineNo = lineNo
    self.content = content.strip()

    self.variant = variant
    self.lineParts = self.__parse(self.content)
    if not self.lineParts:
      Logger.fail("Empty check line", self.fileName, self.lineNo)

    if self.variant == CheckLine.Variant.Not:
      for elem in self.lineParts:
        if elem.variant == CheckElement.Variant.VarDef:
          Logger.fail("CHECK-NOT lines cannot define variables", self.fileName, self.lineNo)

  def __eq__(self, other):
    return (isinstance(other, self.__class__) and
            self.variant == other.variant and
            self.lineParts == other.lineParts)

  # Returns True if the given Match object was at the beginning of the line.
  def __isMatchAtStart(self, match):
    return (match is not None) and (match.start() == 0)

  # Takes in a list of Match objects and returns the minimal start point among
  # them. If there aren't any successful matches it returns the length of
  # the searched string.
  def __firstMatch(self, matches, string):
    starts = map(lambda m: len(string) if m is None else m.start(), matches)
    return min(starts)

  # This method parses the content of a check line stripped of the initial
  # comment symbol and the CHECK keyword.
  def __parse(self, line):
    lineParts = []
    # Loop as long as there is something to parse.
    while line:
      # Search for the nearest occurrence of the special markers.
      matchWhitespace = re.search(r"\s+", line)
      matchPattern = re.search(CheckElement.regexPattern, line)
      matchVariable = re.search(CheckElement.regexVariable, line)

      # If one of the above was identified at the current position, extract them
      # from the line, parse them and add to the list of line parts.
      if self.__isMatchAtStart(matchWhitespace):
        # A whitespace in the check line creates a new separator of line parts.
        # This allows for ignored output between the previous and next parts.
        line = line[matchWhitespace.end():]
        lineParts.append(CheckElement.newSeparator())
      elif self.__isMatchAtStart(matchPattern):
        pattern = line[0:matchPattern.end()]
        line = line[matchPattern.end():]
        lineParts.append(CheckElement.parsePattern(pattern))
      elif self.__isMatchAtStart(matchVariable):
        var = line[0:matchVariable.end()]
        line = line[matchVariable.end():]
        lineParts.append(CheckElement.parseVariable(var))
      else:
        # If we're not currently looking at a special marker, this is a plain
        # text match all the way until the first special marker (or the end
        # of the line).
        firstMatch = self.__firstMatch([ matchWhitespace, matchPattern, matchVariable ], line)
        text = line[0:firstMatch]
        line = line[firstMatch:]
        lineParts.append(CheckElement.parseText(text))
    return lineParts

  # Returns the regex pattern to be matched in the output line. Variable
  # references are substituted with their current values provided in the
  # 'varState' argument.
  # An exception is raised if a referenced variable is undefined.
  def __generatePattern(self, linePart, varState):
    if linePart.variant == CheckElement.Variant.VarRef:
      try:
        return re.escape(varState[linePart.name])
      except KeyError:
        Logger.testFailed("Use of undefined variable \"" + linePart.name + "\"",
                          self.fileName, self.lineNo)
    else:
      return linePart.pattern

  def __isSeparated(self, outputLine, matchStart):
    return (matchStart == 0) or (outputLine[matchStart - 1:matchStart].isspace())

  # Attempts to match the check line against a line from the output file with
  # the given initial variable values. It returns the new variable state if
  # successful and None otherwise.
  def match(self, outputLine, initialVarState):
    # Do the full matching on a shadow copy of the variable state. If the
    # matching fails half-way, we will not need to revert the state.
    varState = dict(initialVarState)

    matchStart = 0
    isAfterSeparator = True

    # Now try to parse all of the parts of the check line in the right order.
    # Variable values are updated on-the-fly, meaning that a variable can
    # be referenced immediately after its definition.
    for part in self.lineParts:
      if part.variant == CheckElement.Variant.Separator:
        isAfterSeparator = True
        continue

      # Find the earliest match for this line part.
      pattern = self.__generatePattern(part, varState)
      while True:
        match = re.search(pattern, outputLine[matchStart:])
        if (match is None) or (not isAfterSeparator and not self.__isMatchAtStart(match)):
          return None
        matchEnd = matchStart + match.end()
        matchStart += match.start()

        # Check if this is a valid match if we expect a whitespace separator
        # before the matched text. Otherwise loop and look for another match.
        if not isAfterSeparator or self.__isSeparated(outputLine, matchStart):
          break
        else:
          matchStart += 1

      if part.variant == CheckElement.Variant.VarDef:
        if part.name in varState:
          Logger.testFailed("Multiple definitions of variable \"" + part.name + "\"",
                            self.fileName, self.lineNo)
        varState[part.name] = outputLine[matchStart:matchEnd]

      matchStart = matchEnd
      isAfterSeparator = False

    # All parts were successfully matched. Return the new variable state.
    return varState

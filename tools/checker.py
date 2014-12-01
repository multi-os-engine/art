#!/usr/bin/env python3
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


# Checker is a testing tool which compiles a given test file and compares the
# state of the control-flow graph before and after each optimization pass
# against a set of assertions specified alongside the tests.
#
# Tests are written in Java, turned into DEX and compiled with the Optimizing
# compiler. "Check lines" are comments in the Java file which begin with prefix
# 'CHECK' followed by a pattern that the engine attempts to match in the
# compiler-generated output.
#
# Assertions are tested in groups which correspond to the individual compiler
# passes. Each group of check lines therefore must start with a 'CHECK-START'
# header which specifies the output group it should be tested against. The group
# name must exactly match one of the groups recognized in the output (they can
# be listed with a command-line flag).
#
# Check line patterns are treated as plain text rather than regular expressions
# but are whitespace agnostic.
#
# Actual regex patterns can be inserted enclosed in '{{' and '}}' brackets. If
# curly brackets need to be used inside the body of the regex, they may need to
# be enclosed in round brackets. For example, the pattern '{{foo{2}}}' will fail
# because the regex 'foo{2' is invalid, but '{{(fo{2})}}' will match 'foo'.
#
# Regex patterns can be named and referenced later. A new variable is defined
# with '[[name:regex]]' and can be referenced with '[[name]]'. Variables are
# only valid within the scope of the defining group. Within a group they cannot
# be redefined or used undefined.
#
# Example:
#   The following assertions can be placed in a Java source file:
#
#   // CHECK-START: int MyClass.MyMethod() constant_folding (after)
#   // CHECK:         [[ID:i[0-9]+]] IntConstant {{11|22}}
#   // CHECK:                        Return [ [[ID]] ]
#
#   The engine will attempt to match the check lines against the output of the
#   group named on the first line. Together they verify that the CFG after
#   constant folding returns an integer constant with value either 11 or 22.
#

import argparse
import os
import re
import shutil
import sys
import tempfile
from subprocess import check_call

class CommonEqualityMixin:
  """Mixin for class equality as equality of the fields."""
  def __eq__(self, other):
    return (isinstance(other, self.__class__)
           and self.__dict__ == other.__dict__)

  def __ne__(self, other):
    return not self.__eq__(other)

  def __repr__(self):
    return "<%s: %s>" % (type(self).__name__, str(self.__dict__))

class RegexVariant(object):
  """Enumeration of the supported language constructs."""
  Text, Pattern, VarRef, VarDef = range(4)


class RegexElement(CommonEqualityMixin):
  """Single element of the check line."""
  def __init__(self, variant, name, pattern):
    self.variant = variant
    self.name = name
    self.pattern = pattern


class CheckLine(CommonEqualityMixin):
  """Representation of a single assertion in the check file formed of one or
     more regex elements. Matching against an output line is successful only
     if all regex elements can be matched in the given order."""

  def __init__(self, lineContent, lineNo=-1):
    lineContent = lineContent.strip()

    self.lineNo = lineNo
    self.content = lineContent

    self.lineParts = self.__parseCheckLine(lineContent)
    if len(self.lineParts) == 0:
      raise Exception("Empty check line")

  # Returns True if the given Match object was at the beginning of the line.
  def __isMatchAtStart(self, match):
    return (match is not None) and (match.start() == 0)

  # Takes in a list of Match objects and returns the minimal start point among
  # them. If there aren't any successful matches it returns the length of
  # the searched string.
  def __firstMatch(self, matches, string):
    starts = map(lambda m: len(string) if m is None else m.start(), matches)
    return min(starts)

  # Returns the regex for finding a regex pattern in the check line.
  def __getPatternRegex(self):
    rStartSym = "\{\{"
    rEndSym = "\}\}"
    rBody = ".+?"
    return rStartSym + rBody + rEndSym

  # Returns the regex for finding a variable use in the check line.
  def __getVariableRegex(self):
    rStartSym = "\[\["
    rEndSym = "\]\]"
    rStartOptional = "("
    rEndOptional = ")?"
    rName = "[a-zA-Z][a-zA-Z0-9]*"
    rSeparator = ":"
    rBody = ".+?"
    return rStartSym + rName + rStartOptional + rSeparator + rBody + rEndOptional + rEndSym

  # Returns a RegexPart object containing a regex pattern that matches the given
  # plain text.
  def __parseText(self, text):
    return RegexElement(RegexVariant.Text, None, re.escape(text))

  # Returns a RegexPart object defining a regex pattern.
  def __parsePattern(self, pattern):
    return RegexElement(RegexVariant.Pattern, None, pattern[2:len(pattern)-2])

  # Returns a RegexPart object that hold a variable use (either a reference or
  # a definition).
  def __parseVariable(self, var):
    colonPos = var.find(":")
    if colonPos == -1:
      # Variable reference
      name = var[2:len(var)-2]
      return RegexElement(RegexVariant.VarRef, name, None)
    else:
      # Variable definition
      name = var[2:colonPos]
      body = var[colonPos+1:len(var)-2]
      return RegexElement(RegexVariant.VarDef, name, body)

  # This method parses the content of a check line stripped of the initial
  # comment symbol and the CHECK keyword.
  def __parseCheckLine(self, line):
    lineParts = []
    # Loop as long as there is something to parse.
    while line:
      # Search for the nearest occurance of the special markers.
      matchWhitespace = re.search("\s+", line)
      matchPattern = re.search(self.__getPatternRegex(), line)
      matchVariable = re.search(self.__getVariableRegex(), line)

      # If one of the above was identified at the current position, extract them
      # from the line, parse them and add to the list of line parts.
      if self.__isMatchAtStart(matchWhitespace):
        # We want to be whitespace-agnostic so whenever a check line contains
        # a whitespace, we add a regex pattern for an arbitrary non-zero number
        # of whitespaces.
        line = line[matchWhitespace.end():]
        lineParts.append(self.__parsePattern("{{\s+}}"))
      elif self.__isMatchAtStart(matchPattern):
        pattern = line[0:matchPattern.end()]
        line = line[matchPattern.end():]
        lineParts.append(self.__parsePattern(pattern))
      elif self.__isMatchAtStart(matchVariable):
        var = line[0:matchVariable.end()]
        line = line[matchVariable.end():]
        lineParts.append(self.__parseVariable(var))
      else:
        # If we're not currently looking at a special marker, this is a plain
        # text match all the way until the first special marker (or the end
        # of the line if none are present).
        firstMatch = self.__firstMatch([ matchWhitespace, matchPattern, matchVariable ], line)
        text = line[0:firstMatch]
        line = line[firstMatch:]
        lineParts.append(self.__parseText(text))
    return lineParts

  # Returns the regex pattern to be matched in the output line. Variable
  # references are substituted with their current values provided in the
  # 'varState' argument.
  # An exception is raised if a referenced variable is undefined.
  def __generatePattern(self, linePart, varState):
    if linePart.variant == RegexVariant.VarRef:
      try:
        return re.escape(varState[linePart.name])
      except KeyError:
        raise Exception("ERROR: Variable '" + linePart.name + "' undefined on check line " +
                        str(self.lineNo))
    else:
      return linePart.pattern

  # Attempts to match the check line against a line from the output file with
  # the given initial variable values. It returns the new variable state if
  # successful and None otherwise.
  def match(self, outputLine, initialVarState):
    initialSearchFrom = 0
    initialPattern = self.__generatePattern(self.lineParts[0], initialVarState)
    while True:
      # Search for the first element on the regex parts list. This will mark
      # the point on the line from which we will attempt to match the rest of
      # the check pattern. If this iteration produces only a partial match,
      # the next iteration will starting searching further in the output.
      firstMatch = re.search(initialPattern, outputLine[initialSearchFrom:])
      if firstMatch is None:
        return None
      matchStart = initialSearchFrom + firstMatch.start()
      initialSearchFrom += firstMatch.start() + 1

      # Do the full matching on a shadow copy of the variable state. If the
      # matching fails half-way, we will not need to revert the state.
      varState = dict(initialVarState)

      # Now try to parse all of the parts of the check line in the right order.
      # Variable values are updated on-the-fly, meaning that a variable can
      # be referenced immediately after its definition.
      fullyMatched = True
      for part in self.lineParts:
        pattern = self.__generatePattern(part, varState)
        match = re.match(pattern, outputLine[matchStart:])
        if match is None:
          fullyMatched = False
          break
        matchEnd = matchStart + match.end()
        if part.variant == RegexVariant.VarDef:
          if part.name in varState:
            raise Exception("Redefinition of variable " + part.name)
          varState[part.name] = outputLine[matchStart:matchEnd]
        matchStart = matchEnd

      # Return the new variable state if all parts were successfully matched.
      # Otherwise loop and try to find another start point on the same line.
      if fullyMatched:
        return varState


class OutputGroup(CommonEqualityMixin):
  """Represents a named part of the test output against which a check group of
     the same name is to be matched."""

  def __init__(self, name, body):
    if name:
      self.name = name
    else:
      raise Exception("Output group does not have a name")
    if body:
      self.body = body
    else:
      raise Exception("Output group " + self.name + " does not have a body")


class CheckGroup(CommonEqualityMixin):
  """Represents a named collection of check lines which are to be matched
     against an output group of the same name."""

  def __init__(self, name, lines):
    if name:
      self.name = name
    else:
      raise Exception("Check group does not have a name")
    if lines:
      self.lines = lines
    else:
      raise Exception("Check group " + self.name + " does not have a body")

  def __pop(self, list):
    return list[0], list[1:]

  # The driver of matching inside a group. It simultaneously reads lines from
  # the output and check groups and attempts to match them against each other
  # in the correct order.
  def match(self, outputGroup):
    readOutputLines = 0
    lastMatch = 0

    # Check and output lines which remain to be matched
    checkLines = self.lines
    outputLines = outputGroup.body
    varState = {}

    # Retrieve the next check line
    while checkLines:
      checkLine, checkLines = self.__pop(checkLines)
      foundMatch = False

      # Retrieve the next output line
      while outputLines:
        outputLine, outputLines = self.__pop(outputLines)
        readOutputLines += 1

        # Try to match the current lines against each other. If successful,
        # save the new state of variables and continue to the next check line.
        newVarState = checkLine.match(outputLine, varState)
        if newVarState is not None:
          varState = newVarState
          lastMatch = readOutputLines
          foundMatch = True
          break
      if not foundMatch:
        raise Exception("Could not match check line \"" + checkLine.content + "\" from line " +
                        str(lastMatch+1) + " of the output. [vars=" + str(varState) + "]")

  @staticmethod
  def parse(name, lines):
    return CheckGroup(name, list(map(lambda line: CheckLine(line), lines)))

class FileSplit(object):
  """Base class for representing text files which need to be split into smaller
     chunks before being parsed."""

  def __init__(self, stream):
    lineNo = 0
    allGroups = []
    currentGroup = None

    for line in stream:
      lineNo += 1
      # Let the child class process the line and return information about it.
      # The _processLine method can modify the content of the line (or delete it
      # entirely) and specify whether it starts a new group.
      processedLine, newGroupName = self._processLine(line.strip(), lineNo)
      if newGroupName:
        currentGroup = (newGroupName.strip(), [])
        allGroups.append(currentGroup)
      if processedLine:
        currentGroup[1].append(processedLine.strip())

    # Finally, take the generated line groups and let the child class process
    # each one before storing the final outcome.
    self.groups = list(map(lambda group: self._processGroup(group[0], group[1]), allGroups))


class OutputFile(FileSplit):
  """Representation of the output generated by the test and split into groups
     within which the checks are performed.

     Stream in C1visualizer format is parsed with a state machine which
     differentiates between the 'compilation' and 'cfg' blocks. The former
     marks the beginning of a method, is parsed for the method's name but
     otherwise ignored. Each subsequent 'cfg' block represents one stage of
     the compilation pipeline and is parsed into an output group named
     "<method name> <pass name>".
     """

  class ParsingState:
    OutsideBlock, InsideCompilationBlock, StartingCfgBlock, InsideCfgBlock = range(4)

  def __init__(self, outputStream):
    # Initialize the state machine
    self.lastMethodName = None
    self.state = OutputFile.ParsingState.OutsideBlock
    # Initiate the parsing
    FileSplit.__init__(self, outputStream)

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
        raise Exception("Expected group name in output file")

    elif self.state == OutputFile.ParsingState.InsideCfgBlock:
      if line == "end_cfg":
        self.state = OutputFile.ParsingState.OutsideBlock
        return (None, None)
      else:
        return (line, None)

    elif self.state == OutputFile.ParsingState.InsideCompilationBlock:
      # Search for the method's name. Format: method "<name>"
      if re.match("method\s+\"[^\"]+\"", line):
        self.lastMethodName = line.split("\"")[1]
      elif line == "end_compilation":
        self.state = OutputFile.ParsingState.OutsideBlock
      return (None, None)

    else:  # self.state == OutputFile.ParsingState.OutsideBlock:
      if line == "begin_cfg":
        # The line starts a new group but we'll wait until the next line from
        # which we can extract the name of the pass.
        if self.lastMethodName is None:
          raise Exception("Output contains a pass without a method header")
        self.state = OutputFile.ParsingState.StartingCfgBlock
        return (None, None)
      elif line == "begin_compilation":
        self.state = OutputFile.ParsingState.InsideCompilationBlock
        return (None, None)
      else:
        raise Exception("Output line " + str(lineNo) + " \"" + line + "\" outside a group")

  def _processGroup(self, name, lines):
    return OutputGroup(name, lines)

  def findGroup(self, name):
    for group in self.groups:
      if group.name == name:
        return group
    return None


class CheckFile(FileSplit):
  """Collection of check groups extracted from the input test file."""

  def __init__(self, prefix, checkStream):
    self.prefix = prefix
    FileSplit.__init__(self, checkStream)

  # Attempts to parse a check line. The regex searches for a comment symbol
  # followed by the CHECK keyword, given attribute and a colon at the very
  # beginning of the line. Whitespaces are ignored.
  def _extractLine(self, prefix, line):
    ignoreWhitespace = "\s*"
    commentSymbols = ["//", "#"]
    prefixRegex = ignoreWhitespace + \
                  "(" + "|".join(commentSymbols) + ")" + \
                  ignoreWhitespace + \
                  prefix + ":"

    # The 'match' function succeeds only if the pattern is matched at the
    # beginning of the line.
    match = re.match(prefixRegex, line)
    if match is not None:
      return line[match.end():]
    else:
      return None

  def _processLine(self, line, lineNo):
    startLine = self._extractLine(self.prefix + "-START", line)
    if startLine is not None:
      # Line starts with the CHECK-START keyword, start a new group
      return (None, startLine)
    else:
      # Otherwise try to parse it as a standard CHECK line. If unsuccessful,
      # _extractLine will return None and the line will be ignored.
      return (self._extractLine(self.prefix, line), None)

  def _exceptionLineOutsideGroup(self, line, lineNo):
    raise Exception("Check file line " + str(lineNo) + " outside group")

  def _processGroup(self, name, lines):
    return CheckGroup.parse(name, lines)

  def match(self, outputFile, printInfo=False):
    for CheckGroup in self.groups:
      # TODO: Currently does not handle multiple occurances of the same group
      # name, e.g. when a pass is run multiple times. It will always try to
      # match a check group against the first output group of the same name.
      OutputGroup = outputFile.findGroup(CheckGroup.name)
      if OutputGroup is None:
        raise Exception("Group " + CheckGroup.name + " not found")
      if printInfo:
        print("TEST " + CheckGroup.name + "... ", end="", flush=True)
      try:
        CheckGroup.match(OutputGroup)
        if printInfo:
          print("PASSED")
      except Exception as e:
        if printInfo:
          print("FAILED!")
        raise e

class cd:
  """Helper class which temporarily changes the working directory."""

  def __init__(self, newPath):
    self.newPath = newPath

  def __enter__(self):
    self.savedPath = os.getcwd()
    os.chdir(self.newPath)

  def __exit__(self, etype, value, traceback):
    os.chdir(self.savedPath)


def ParseArguments():
  parser = argparse.ArgumentParser()
  parser.add_argument("test_file")
  parser.add_argument("--check-prefix", dest="check_prefix", default="CHECK")
  parser.add_argument("--list-groups", dest="list_groups", action='store_true')
  parser.add_argument("--dump-group", dest="dump_group")
  return parser.parse_args()

def CompileTest(inputFile, tempFolder):
  classFolder = tempFolder + "/classes"
  dexFile = tempFolder + "/test.dex"
  oatFile = tempFolder + "/test.oat"
  outputFile = tempFolder + "/art.cfg"
  os.makedirs(classFolder)

  # Build a DEX from the source file. We pass "--no-optimize" to dx to avoid
  # interference with its optimizations.
  check_call(["javac", "-d", classFolder, inputFile])
  check_call(["dx", "-JXmx256m", "--dex", "--no-optimize", "--output=" + dexFile, classFolder])

  # Run dex2oat and export the HGraph. The output is stored into ${PWD}/art.cfg.
  with cd(tempFolder):
    check_call(["dex2oat", "-j1", "--dump-passes", "--compiler-backend=Optimizing",
                "--android-root=" + os.environ["ANDROID_HOST_OUT"],
                "--boot-image=" + os.environ["ANDROID_HOST_OUT"] + "/framework/core-optimizing.art",
                "--runtime-arg", "-Xnorelocate", "--dex-file=" + dexFile, "--oat-file=" + oatFile])

  return outputFile

def ListGroups(outputFilename):
  outputFile = OutputFile(open(outputFilename, "r"))
  for group in outputFile.groups:
    print(group.name)

def DumpGroup(outputFilename, groupName):
  outputFile = OutputFile(open(outputFilename, "r"))
  group = outputFile.findGroup(groupName)
  if group:
    print("\n".join(group.body))
  else:
    raise Exception("Group " + groupName + " not found")

def RunChecks(checkPrefix, checkFilename, outputFilename):
  checkFile = CheckFile(checkPrefix, open(checkFilename, "r"))
  outputFile = OutputFile(open(outputFilename, "r"))
  checkFile.match(outputFile, True)

if __name__ == "__main__":
  args = ParseArguments()
  tempFolder = tempfile.mkdtemp()

  try:
    outputFile = CompileTest(args.test_file, tempFolder)
    if args.list_groups:
      ListGroups(outputFile)
    elif args.dump_group:
      DumpGroup(outputFile, args.dump_group)
    else:
      RunChecks(args.check_prefix, args.test_file, outputFile)
  finally:
    shutil.rmtree(tempFolder)

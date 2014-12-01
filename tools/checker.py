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


# This script compares two files: a check file and an output file. Check file is
# the source file of a test and it contains the expected output in its comments.
# The "check-lines" begin with the 'CHECK' prefix followed by a pattern that is
# searched for in the output file. They are matched in the order of appearance
# in the check file.
#
# Check patterns are treated as plain text rather than regular expressions but
# they are whitespace agnostic.
#
# Regex patterns can be inserted enclosed in '{{' and '}}' brackets. If these
# need to be used inside the body of the regex, they need to be separated with
# round brackets, e.g. '{{(fo{2})}}' will match 'foo'.
#
# Regex patterns can be named and referenced later. A new variable is defined
# with '[[name:regex]]' and can be referenced with '[[name]]'.
#
# Example:
#   The following two check lines can be placed in a Java source file:
#
#   // CHECK: add [[REG:v[0-9]+]], {{v[0-9]+}}, {{v[0-9]+}}
#   // CHECK: return [[REG]]
#
#   Provided the output file is a smali dump, these checks will match the output
#   if it contains an 'add' instruction whose destination register matches the
#   register referenced by a 'return' instruction further in the file.

import argparse
import os
import re
import shutil
import sys
import tempfile
from subprocess import check_call

class RegexVariant:
  Text, Pattern, VarRef, VarDef = range(4)

class Regex:
  def __init__(self, variant, name, pattern):
    self.variant = variant
    self.name = name
    self.pattern = pattern

class CheckLine:
  def __init__(self, lineContent, lineNo=-1):
    lineContent = lineContent.strip()

    self.lineNo = lineNo
    self.content = lineContent

    self.lineParts = self._parseCheckLine(lineContent)
    if len(self.lineParts) == 0:
      raise Exception("Empty check line")

  # Returns True if the given Match object was at the beginning of the line.
  def _isMatchAtStart(self, match):
    return (match is not None) and (match.start() == 0)

  # Takes in a list of Match objects and returns the minimal start point among
  # them. If there aren't any successful matches it returns the length of
  # the searched string.
  def _firstMatch(self, matches, string):
    starts = map(lambda m: len(string) if m is None else m.start(), matches)
    return min(starts)

  # Returns the regex for finding a regex pattern in the check line.
  def _getPatternRegex(self):
    rStartSym = "\{\{"
    rEndSym = "\}\}"
    rBody = ".+?"
    return rStartSym + rBody + rEndSym

  # Returns the regex for finding a variable use in the check line.
  def _getVariableRegex(self):
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
  def _parseText(self, text):
    return Regex(RegexVariant.Text, None, re.escape(text))

  # Returns a RegexPart object defining a regex pattern.
  def _parsePattern(self, pattern):
    return Regex(RegexVariant.Pattern, None, pattern[2:len(pattern)-2])

  # Returns a RegexPart object that hold a variable use (either a reference or
  # a definition).
  def _parseVariable(self, var):
    colonPos = var.find(":")
    if colonPos == -1:
      # Variable reference
      name = var[2:len(var)-2]
      return Regex(RegexVariant.VarRef, name, None)
    else:
      # Variable definition
      name = var[2:colonPos]
      body = var[colonPos+1:len(var)-2]
      return Regex(RegexVariant.VarDef, name, body)

  # This method parses the content of a check line stripped of the initial
  # comment symbol and the CHECK keyword.
  def _parseCheckLine(self, line):
    lineParts = []
    # Loop as long as there is something to parse.
    while line:
      # Search for the nearest occurance of the special markers.
      matchWhitespace = re.search("\s+", line)
      matchPattern = re.search(self._getPatternRegex(), line)
      matchVariable = re.search(self._getVariableRegex(), line)

      # If one of the above was identified at the current position, extract them
      # from the line, parse them and add to the list of line parts.
      if self._isMatchAtStart(matchWhitespace):
        # We want to be whitespace-agnostic so whenever a check line contains
        # a whitespace, we add a regex pattern for an arbitrary non-zero number
        # of whitespaces.
        line = line[matchWhitespace.end():]
        lineParts.append(self._parsePattern("{{\s+}}"))
      elif self._isMatchAtStart(matchPattern):
        pattern = line[0:matchPattern.end()]
        line = line[matchPattern.end():]
        lineParts.append(self._parsePattern(pattern))
      elif self._isMatchAtStart(matchVariable):
        var = line[0:matchVariable.end()]
        line = line[matchVariable.end():]
        lineParts.append(self._parseVariable(var))
      else:
        # If we're not currently looking at a special marker, this is a plain
        # text match all the way until the first special marker (or the end
        # of the line if none are present).
        firstMatch = self._firstMatch([ matchWhitespace, matchPattern, matchVariable ], line)
        text = line[0:firstMatch]
        line = line[firstMatch:]
        lineParts.append(self._parseText(text))
    return lineParts

  # Returns the regex pattern to be matched in the output line. Variable
  # references are substituted with their current values provided in the
  # 'varState' argument.
  # An exception is raised if a referenced variable is undefined.
  def _generatePattern(self, linePart, varState):
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
    initialPattern = self._generatePattern(self.lineParts[0], initialVarState)
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
        pattern = self._generatePattern(part, varState)
        match = re.match(pattern, outputLine[matchStart:])
        if match is None:
          fullyMatched = False
          break
        matchEnd = matchStart + match.end()
        if part.variant == RegexVariant.VarDef:
          varState[part.name] = outputLine[matchStart:matchEnd]
        matchStart = matchEnd

      # Return the new variable state if all parts were successfully matched.
      # Otherwise loop and try to find another start point on the same line.
      if fullyMatched:
        return varState


class CheckFile:
  def __init__(self, checkPrefix, checkStream, extraCheckLines=[]):
    self.checkPrefix = checkPrefix
    self.checkLines = []
    self._parseFile(checkStream)
    self.checkLines.extend(extraCheckLines)

  # Reads lines from the check file and attempts to parse them one by one.
  def _parseFile(self, checkStream):
    lineNo = 0
    for line in checkStream:
      lineNo += 1
      checkLine = self._parseLine(line, lineNo)
      if checkLine is not None:
        self.checkLines.append(checkLine)

  # Attempts to parse a check line. The regex searches for a comment symbol
  # followed by the CHECK keyword and a colon at the very beginning of the line.
  # Whitespaces are ignored.
  def _parseLine(self, line, linenum):
    ignoreWhitespace = "\s*"
    commentSymbols = ["//", "#"]
    prefixRegex = ignoreWhitespace + \
                  "(" + "|".join(commentSymbols) + ")" + \
                  ignoreWhitespace + \
                  self.checkPrefix + ":"

    # The 'match' function succeeds only if the pattern is matched at the
    # beginning of the line.
    match = re.match(prefixRegex, line)
    if match is not None:
      return CheckLine(line[match.end():], linenum)
    else:
      return None

  # Reads from the output file and attempts to match the check lines in their
  # given order.
  def match(self, outputStream):
    readOutputLines = 0
    lastMatch = 0
    varState = {}
    while self.checkLines:
      checkLine = self.checkLines.pop(0)
      newVarState = None
      for outputLine in outputStream:
        readOutputLines += 1
        newVarState = checkLine.match(outputLine, varState)
        if newVarState is not None:
          break

      if newVarState is not None:
        varState = newVarState
        lastMatch = readOutputLines
      else:
        raise Exception("ERROR: Could not match check line " + str(checkLine.lineNo) + \
                        " '" + checkLine.content + "' " + "starting from output line " + \
                        str(lastMatch + 1) + ", varState=" + str(varState))


def ParseArguments():
  parser = argparse.ArgumentParser()
  parser.add_argument("input_file")
  parser.add_argument("--check-prefix", dest="check_prefix", default="CHECK")
  parser.add_argument("--dump-only", dest="dump_only", action='store_true')
  return parser.parse_args()

def CompileTest(inputFile, tempFolder):
  classFolder = tempFolder + "/classes"
  dexFile = tempFolder + "/test.dex"
  oatFile = tempFolder + "/test.oat"
  outputFile = tempFolder + "/test.dump"
  os.makedirs(classFolder)

  # Build a DEX from the source file
  check_call(["javac", "-d", classFolder, inputFile])
  check_call(["dx", "-JXmx256m", "--dex", "--no-optimize", "--output=" + dexFile, classFolder])

  # Run dex2oat and export the HGraph
  outputStream = open(outputFile, "w")
  check_call(["dex2oat", "--dump-passes", "--compiler-backend=Optimizing",
              "--android-root=" + os.environ["ANDROID_HOST_OUT"],
              "--boot-image=" + os.environ["ANDROID_HOST_OUT"] + "/framework/core-optimizing.art",
              "--runtime-arg", "-Xnorelocate", "--dex-file=" + dexFile, "--oat-file=" + oatFile],
             stderr=outputStream)
  outputStream.close()
  return outputFile

def RunChecks(inputFile, outputFile, checkPrefix):
  with open(inputFile, "r") as checkStream:
    checkFile = CheckFile(checkPrefix, checkStream)
  with open(outputFile, "r") as outputStream:
    checkFile.match(outputStream)

def PrintFile(file):
  with open(file, "r") as fin:
    print(fin.read())

if __name__ == "__main__":
  args = ParseArguments()
  tempFolder = tempfile.mkdtemp()

  try:
    outputFile = CompileTest(args.input_file, tempFolder)
    if args.dump_only:
      PrintFile(outputFile)
    else:
      RunChecks(args.input_file, outputFile, args.check_prefix)
      print("SUCCESS")
  finally:
    shutil.rmtree(tempFolder)

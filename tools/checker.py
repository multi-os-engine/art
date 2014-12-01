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


# This script compares two files: a check file and an output file. Check file
# is typically the source file of a test which contains the expected output
# in its comments. These so-called check lines begin with a given prefix
# ('CHECK' by default) followed by a pattern that is searched for in the output
# file. They are matched in the order of appearance in the check file.
#
# Check patterns are treated as plain text rather than regular expressions
# but they are whitespace agnostic.
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
#   if it contains an 'add' instruction whose destination register matches
#   the register referenced by a 'return' instruction further in the file.

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
  variant = None
  name = None
  pattern = None

  def __init__(self, variant, name, pattern):
    self.variant = variant
    self.name = name
    self.pattern = pattern

class CheckLine:

  def __init__(self, lineContent, lineNo=-1):
    lineContent = lineContent.strip()

    self.lineNo = lineNo
    self.content = lineContent

    self.regexParts = self._parseCheckLine(lineContent)
    if len(self.regexParts) == 0:
      raise Exception("Empty check line regex pattern")

  # Returns True if the given Match object was at the beginning of the line.
  def _isMatchAtStart(self, match):
    return (match is not None) and (match.start() == 0)

  # Takes in a list of Match objects and returns the minimal start point among
  # them. If there aren't any successful matches it returns the length of
  # the searched string.
  def _firstMatchStart(self, matches, string):
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
    return (rStartSym + \
            rName + \
            rStartOptional + rSeparator + rBody + rEndOptional + \
            rEndSym)

  # Returns a RegexPart object containing the given plain text but escaped
  # for use as a regular expression.
  def _parseText(self, text):
    return Regex(RegexVariant.Text, None, re.escape(text))

  # Returns a RegexPart object that holds a regex pattern specified in
  # check line.
  def _parsePattern(self, pattern):
    return Regex(RegexVariant.Pattern, None, pattern[2:len(pattern)-2])

  # Returns a RegexPart object that hold a variable use (either a reference
  # or a definition).
  def _parseVariable(self, var):
    colonPos = var.find(":")
    if colonPos == -1: # Variable reference
      name = var[2:len(var)-2]
      return Regex(RegexVariant.VarRef, name, None)
    else: # Variable definition
      name = var[2:colonPos]
      body = var[colonPos+1:len(var)-2]
      body = body.replace("\\<", "<").replace("\\>", ">")
      return Regex(RegexVariant.VarDef, name, body)

  # This method parses the content of a check line stripped of the initial
  # CHECK keyword. It starts from the beginning and looks for the nearest
  # whitespace/pattern/variable in the text. These are parsed in a suitable
  # manner and stretches of plain text between are escaped. These individual
  # parts are stored in the self.regexParts list.
  def _parseCheckLine(self, line):
    regexParts = []
    while line:
      matchWhitespace = re.search("\s+", line)
      matchPattern = re.search(self._getPatternRegex(), line)
      matchVariable = re.search(self._getVariableRegex(), line)

      if self._isMatchAtStart(matchWhitespace):
        line = line[matchWhitespace.end():]
        regexParts.append(self._parsePattern("{{\s+}}"))
      elif self._isMatchAtStart(matchPattern):
        pattern = line[0:matchPattern.end()]
        line = line[matchPattern.end():]
        regexParts.append(self._parsePattern(pattern))
      elif self._isMatchAtStart(matchVariable):
        var = line[0:matchVariable.end()]
        line = line[matchVariable.end():]
        regexParts.append(self._parseVariable(var))
      else:
        firstMatch = \
          self._firstMatchStart([ matchWhitespace, 
                                  matchPattern,
                                  matchVariable ], line)
        text = line[0:firstMatch]
        line = line[firstMatch:]
        regexParts.append(self._parseText(text))
    return regexParts

  # Returns the regex pattern to be matched in the output line given.
  # Variable references are substituted according to their value in the
  # current environment. This method raises an exception if a variable name
  # is not defined in the environment.
  def _generatePattern(self, regexPart, env):
    if regexPart.variant == RegexVariant.VarRef:
      try:
        return env[regexPart.name]
      except KeyError:
        raise Exception("ERROR: Variable '" + regexPart.name + \
                        "' undefined on " + "check line " + \
                        str(self.lineNo))
    else:
      return regexPart.pattern

  # Attempts to match the check line against a line from the output file
  # in the given environment. It returns the new environment when successful
  # and None otherwise.
  # The algorithm will first search for the first element on its regex part
  # list and then attempt to match the rest of the list immediately following
  # the first match. If this is unsuccessful the algorithm repeats, attempting
  # to find another match for the first element.
  # Because the lines are not being matched as a whole but rather by parts,
  # a variable can be referenced on the same line, immediately after it has
  # been defined.
  def match(self, outputLine, initialEnv):
    initialSearchFrom = 0
    initialPattern = self._generatePattern(self.regexParts[0], initialEnv)
    while True:
      firstMatch = re.search(initialPattern, outputLine[initialSearchFrom:])
      if firstMatch is None:
        return None

      matchStart = initialSearchFrom + firstMatch.start()
      env = dict(initialEnv)
      initialSearchFrom += firstMatch.start() + 1

      fullyMatched = True
      for part in self.regexParts:
        pattern = self._generatePattern(part, env)
        match = re.match(pattern, outputLine[matchStart:])
        if match is None:
          fullyMatched = False
          break
        matchEnd = matchStart + match.end()
        if part.variant == RegexVariant.VarDef:
          env[part.name] = outputLine[matchStart:matchEnd]
        matchStart = matchEnd
      
      if fullyMatched:
        return env


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
  # followed by the specified check keyword and a colon at the very beginning
  # of the line. Whitespaces are ignored.
  def _parseLine(self, line, linenum):
    ignoreWhitespace = "\s*"
    commentSymbols = ["//", "#"]
    prefixRegex = ignoreWhitespace + \
                  "(" + "|".join(commentSymbols) + ")" + \
                  ignoreWhitespace + \
                  self.checkPrefix + ":"

    # The 'match' function succeeds iff the pattern is matched from the start
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
    env = {}
    while self.checkLines:
      checkLine = self.checkLines.pop(0)
      newEnv = None
      for outputLine in outputStream:
        readOutputLines += 1
        newEnv = checkLine.match(outputLine, env)
        if newEnv is not None:
          break

      if newEnv is not None:
        env = newEnv
        lastMatch = readOutputLines
      else:
        raise Exception("ERROR: Could not match check line " + \
                        str(checkLine.lineNo) + \
                        " '" + checkLine.content + "' " + \
                        "starting from output line " + \
                        str(lastMatch + 1) + ", env=" + str(env))


def ParseArguments(cl=None):
  parser = argparse.ArgumentParser()
  parser.add_argument("input_file")
  parser.add_argument("--check-prefix", default="CHECK")
  if (cl is None):
    return parser.parse_args()
  else:
    return parser.parse_args(cl.split())


if __name__ == "__main__":
  args = ParseArguments()

  # Prepare the temp folder
  tempFolder = tempfile.mkdtemp()
  try:
    classFolder = tempFolder + "/classes"
    dexFile = tempFolder + "/test.dex"
    oatFile = tempFolder + "/test.oat"
    outputFile = tempFolder + "/test.dump"
    os.makedirs(classFolder)

    # Build a DEX from the source file
    check_call(["javac", "-d", classFolder, args.input_file])
    check_call(["dx", "-JXmx256m", "--dex", "--no-optimize", 
                "--output=" + dexFile, classFolder])

    # Run dex2oat and export the HGraph
    outputStream = open(outputFile, "w")
    os.environ['DEX_LOCATION'] = "my_path"
    check_call(["dex2oat", "--dump-passes", "--compiler-backend=Optimizing", 
                "--android-root=" + os.environ["ANDROID_HOST_OUT"],
                "--boot-image=" + os.environ["ANDROID_HOST_OUT"] + \
                "/framework/core-optimizing.art", "--runtime-arg", "-Xnorelocate",
                "--dex-file=" + dexFile, "--oat-file=" + oatFile],
               stderr=outputStream)
    outputStream.close()

    # Open the input and output files and run the matching algorithm
    checkStream = open(args.input_file, "r")
    checkFile = CheckFile(args.check_prefix, checkStream)
    checkStream.close()

    outputStream = open(outputFile, "r")
    checkFile.match(outputStream)
    outputStream.close()

    print("SUCCESS")

  finally:
    shutil.rmtree(tempFolder)


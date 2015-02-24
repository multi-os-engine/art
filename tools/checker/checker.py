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


# Checker is a testing tool which compiles a given test file and compares the
# state of the control-flow graph before and after each optimization pass
# against a set of assertions specified alongside the tests.
#
# Tests are written in Java, turned into DEX and compiled with the Optimizing
# compiler. "Check lines" are assertions formatted as comments of the Java file.
# They begin with prefix 'CHECK' followed by a pattern that the engine attempts
# to match in the compiler-generated output.
#
# Assertions are tested in groups which correspond to the individual compiler
# passes. Each group of check lines therefore must start with a 'CHECK-START'
# header which specifies the output group it should be tested against. The group
# name must exactly match one of the groups recognized in the output (they can
# be listed with the '--list-groups' command-line flag).
#
# Matching of check lines is carried out in the order of appearance in the
# source file. There are three types of check lines:
#  - CHECK:     Must match an output line which appears in the output group
#               later than lines matched against any preceeding checks. Output
#               lines must therefore match the check lines in the same order.
#               These are referred to as "in-order" checks in the code.
#  - CHECK-DAG: Must match an output line which appears in the output group
#               later than lines matched against any preceeding in-order checks.
#               In other words, the order of output lines does not matter
#               between consecutive DAG checks.
#  - CHECK-NOT: Must not match any output line which appears in the output group
#               later than lines matched against any preceeding checks and
#               earlier than lines matched against any subsequent checks.
#               Surrounding non-negative checks (or boundaries of the group)
#               therefore create a scope within which the assertion is verified.
#
# Check-line patterns are treated as plain text rather than regular expressions
# but are whitespace agnostic.
#
# Actual regex patterns can be inserted enclosed in '{{' and '}}' brackets. If
# curly brackets need to be used inside the body of the regex, they need to be
# enclosed in round brackets. For example, the pattern '{{foo{2}}}' will parse
# the invalid regex 'foo{2', but '{{(fo{2})}}' will match 'foo'.
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

from file import CheckFile, OutputFile
from logger import Logger

import argparse
import os

def ParseArguments():
  parser = argparse.ArgumentParser()
  parser.add_argument("tested_file",
                      help="text file the checks should be verified against")
  parser.add_argument("source_path", nargs="?",
                      help="path to file/folder with checking annotations")
  parser.add_argument("--check-prefix", dest="check_prefix", default="CHECK", metavar="PREFIX",
                      help="prefix of checks in the test files (default: CHECK)")
  parser.add_argument("--list-groups", dest="list_groups", action="store_true",
                      help="print a list of all groups found in the tested file")
  parser.add_argument("--dump-group", dest="dump_group", metavar="GROUP",
                      help="print the contents of an output group")
  parser.add_argument("-q", "--quiet", action="store_true",
                      help="print only errors")
  return parser.parse_args()


def ListGroups(outputFilename):
  outputFile = OutputFile(open(outputFilename, "r"))
  for group in outputFile.groups:
    Logger.log(group.name)


def DumpGroup(outputFilename, groupName):
  outputFile = OutputFile(open(outputFilename, "r"))
  group = outputFile.findGroup(groupName)
  if group:
    lineNo = group.lineNo
    maxLineNo = lineNo + len(group.body)
    lenLineNo = len(str(maxLineNo)) + 2
    for line in group.body:
      Logger.log((str(lineNo) + ":").ljust(lenLineNo) + line)
      lineNo += 1
  else:
    Logger.fail("Group \"" + groupName + "\" not found in the output")


# Returns a list of files to scan for check annotations in the given path. Path
# to a file is returned as a single-element list, directories are recursively
# traversed and all '.java' files returned.
def FindCheckFiles(path):
  if not path:
    Logger.fail("No source path provided")
  elif os.path.isfile(path):
    return [ path ]
  elif os.path.isdir(path):
    foundFiles = []
    for root, dirs, files in os.walk(path):
      for file in files:
        if os.path.splitext(file)[1] == ".java":
          foundFiles.append(os.path.join(root, file))
    return foundFiles
  else:
    Logger.fail("Source path \"" + path + "\" not found")


def RunChecks(checkPrefix, checkPath, outputFilename):
  outputBaseName = os.path.basename(outputFilename)
  outputFile = OutputFile(open(outputFilename, "r"), outputBaseName)

  for checkFilename in FindCheckFiles(checkPath):
    checkBaseName = os.path.basename(checkFilename)
    checkFile = CheckFile(checkPrefix, open(checkFilename, "r"), checkBaseName)
    checkFile.match(outputFile)


if __name__ == "__main__":
  args = ParseArguments()

  if args.quiet:
    Logger.Verbosity = Logger.Level.Error

  if args.list_groups:
    ListGroups(args.tested_file)
  elif args.dump_group:
    DumpGroup(args.tested_file, args.dump_group)
  else:
    RunChecks(args.check_prefix, args.source_path, args.tested_file)

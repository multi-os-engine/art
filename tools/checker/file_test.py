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

from file   import CheckFile, OutputFile
from group  import CheckGroup, OutputGroup
from line   import CheckLine
from logger import Logger

import io
import unittest

def prepareSingleCheck(line):
  if isinstance(line, str):
    return CheckLine(line)
  else:
    return CheckLine(line[0], line[1])

def prepareChecks(lines):
  if isinstance(lines, str):
    lines = lines.splitlines()
  return list(map(lambda line: prepareSingleCheck(line), lines))

class TestCheckFile_PrefixExtraction(unittest.TestCase):
  def __tryParse(self, string):
    checkFile = CheckFile(None, [])
    return checkFile._extractLine("CHECK", string)

  def test_InvalidFormat(self):
    self.assertIsNone(self.__tryParse("CHECK"))
    self.assertIsNone(self.__tryParse(":CHECK"))
    self.assertIsNone(self.__tryParse("CHECK:"))
    self.assertIsNone(self.__tryParse("//CHECK"))
    self.assertIsNone(self.__tryParse("#CHECK"))

    self.assertIsNotNone(self.__tryParse("//CHECK:foo"))
    self.assertIsNotNone(self.__tryParse("#CHECK:bar"))

  def test_InvalidLabel(self):
    self.assertIsNone(self.__tryParse("//ACHECK:foo"))
    self.assertIsNone(self.__tryParse("#ACHECK:foo"))

  def test_NotFirstOnTheLine(self):
    self.assertIsNone(self.__tryParse("A// CHECK: foo"))
    self.assertIsNone(self.__tryParse("A # CHECK: foo"))
    self.assertIsNone(self.__tryParse("// // CHECK: foo"))
    self.assertIsNone(self.__tryParse("# # CHECK: foo"))

  def test_WhitespaceAgnostic(self):
    self.assertIsNotNone(self.__tryParse("  //CHECK: foo"))
    self.assertIsNotNone(self.__tryParse("//  CHECK: foo"))
    self.assertIsNotNone(self.__tryParse("    //CHECK: foo"))
    self.assertIsNotNone(self.__tryParse("//    CHECK: foo"))


class TestCheckFile_Parse(unittest.TestCase):
  def __parsesTo(self, string, expected):
    if isinstance(string, str):
      string = unicode(string)
    checkStream = io.StringIO(string)
    return self.assertEqual(CheckFile("CHECK", checkStream).groups, expected)

  def test_NoInput(self):
    self.__parsesTo(None, [])
    self.__parsesTo("", [])

  def test_SingleGroup(self):
    self.__parsesTo("""// CHECK-START: Example Group
                       // CHECK:  foo
                       // CHECK:    bar""",
                    [ CheckGroup("Example Group", prepareChecks([ "foo", "bar" ])) ])

  def test_MultipleGroups(self):
    self.__parsesTo("""// CHECK-START: Example Group1
                       // CHECK: foo
                       // CHECK: bar
                       // CHECK-START: Example Group2
                       // CHECK: abc
                       // CHECK: def""",
                    [ CheckGroup("Example Group1", prepareChecks([ "foo", "bar" ])),
                      CheckGroup("Example Group2", prepareChecks([ "abc", "def" ])) ])

  def test_CheckVariants(self):
    self.__parsesTo("""// CHECK-START: Example Group
                       // CHECK:     foo
                       // CHECK-NOT: bar
                       // CHECK-DAG: abc
                       // CHECK-DAG: def""",
                    [ CheckGroup("Example Group",
                      prepareChecks([ ("foo", CheckLine.Variant.InOrder),
                                      ("bar", CheckLine.Variant.Not),
                                      ("abc", CheckLine.Variant.DAG),
                                      ("def", CheckLine.Variant.DAG) ])) ])

class TestOutputFile_Parse(unittest.TestCase):
  def __parsesTo(self, string, expected):
    if isinstance(string, str):
      string = unicode(string)
    outputStream = io.StringIO(string)
    return self.assertEqual(OutputFile(outputStream).groups, expected)

  def test_NoInput(self):
    self.__parsesTo(None, [])
    self.__parsesTo("", [])

  def test_SingleGroup(self):
    self.__parsesTo("""begin_compilation
                         method "MyMethod"
                       end_compilation
                       begin_cfg
                         name "pass1"
                         foo
                         bar
                       end_cfg""",
                    [ OutputGroup("MyMethod pass1", [ "foo", "bar" ]) ])

  def test_MultipleGroups(self):
    self.__parsesTo("""begin_compilation
                         name "xyz1"
                         method "MyMethod1"
                         date 1234
                       end_compilation
                       begin_cfg
                         name "pass1"
                         foo
                         bar
                       end_cfg
                       begin_cfg
                         name "pass2"
                         abc
                         def
                       end_cfg""",
                    [ OutputGroup("MyMethod1 pass1", [ "foo", "bar" ]),
                      OutputGroup("MyMethod1 pass2", [ "abc", "def" ]) ])

    self.__parsesTo("""begin_compilation
                         name "xyz1"
                         method "MyMethod1"
                         date 1234
                       end_compilation
                       begin_cfg
                         name "pass1"
                         foo
                         bar
                       end_cfg
                       begin_compilation
                         name "xyz2"
                         method "MyMethod2"
                         date 5678
                       end_compilation
                       begin_cfg
                         name "pass2"
                         abc
                         def
                       end_cfg""",
                    [ OutputGroup("MyMethod1 pass1", [ "foo", "bar" ]),
                      OutputGroup("MyMethod2 pass2", [ "abc", "def" ]) ])

if __name__ == '__main__':
  Logger.Verbosity = Logger.Level.NoOutput
  unittest.main()

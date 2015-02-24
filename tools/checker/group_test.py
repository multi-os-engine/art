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

import unittest

CheckerException = SystemExit

class TestCheckGroup_Match(unittest.TestCase):
  def __matchMulti(self, checkLines, outputString):
    checkGroup = CheckGroup("MyGroup", checkLines)
    outputGroup = OutputGroup("MyGroup", outputString.splitlines())
    return checkGroup.match(outputGroup)

  def __notMatchMulti(self, checkString, outputString):
    with self.assertRaises(CheckerException):
      self.__matchMulti(checkString, outputString)

  def test_TextAndPattern(self):
    self.__matchMulti([ CheckLine("foo bar",     CheckLine.Variant.InOrder),
                        CheckLine("abc {{def}}", CheckLine.Variant.InOrder) ],
                      """foo bar
                         abc def""");
    self.__matchMulti([ CheckLine("foo bar",     CheckLine.Variant.InOrder),
                        CheckLine("abc {{de.}}", CheckLine.Variant.InOrder) ],
                      """=======
                         foo bar
                         =======
                         abc de#
                         =======""");
    self.__notMatchMulti([ CheckLine("//XYZ: foo bar",     CheckLine.Variant.InOrder),
                           CheckLine("//XYZ: abc {{def}}", CheckLine.Variant.InOrder) ],
                         """=======
                            foo bar
                            =======
                            abc de#
                            =======""");

  def test_Variables(self):
    self.__matchMulti([ CheckLine("foo[[X:.]]bar", CheckLine.Variant.InOrder),
                        CheckLine("abc[[X]]def",   CheckLine.Variant.InOrder) ],
                      """foo bar
                         abc def""");
    self.__matchMulti([ CheckLine("foo[[X:([0-9]+)]]bar", CheckLine.Variant.InOrder),
                        CheckLine("abc[[X]]def",          CheckLine.Variant.InOrder),
                        CheckLine("### [[X]] ###",        CheckLine.Variant.InOrder) ],
                      """foo1234bar
                         abc1234def
                         ### 1234 ###""");

  def test_Ordering(self):
    self.__matchMulti([ CheckLine("foo", CheckLine.Variant.InOrder),
                        CheckLine("bar", CheckLine.Variant.InOrder) ],
                      """foo
                         bar""")
    self.__notMatchMulti([ CheckLine("foo", CheckLine.Variant.InOrder),
                           CheckLine("bar", CheckLine.Variant.InOrder) ],
                         """bar
                            foo""")
    self.__matchMulti([ CheckLine("abc", CheckLine.Variant.DAG),
                        CheckLine("def", CheckLine.Variant.DAG) ],
                      """abc
                         def""")
    self.__matchMulti([ CheckLine("abc", CheckLine.Variant.DAG),
                        CheckLine("def", CheckLine.Variant.DAG) ],
                      """def
                         abc""")
    self.__matchMulti([ CheckLine("foo", CheckLine.Variant.InOrder),
                        CheckLine("abc", CheckLine.Variant.DAG),
                        CheckLine("def", CheckLine.Variant.DAG),
                        CheckLine("bar", CheckLine.Variant.InOrder) ],
                      """foo
                         def
                         abc
                         bar""")
    self.__notMatchMulti([ CheckLine("foo", CheckLine.Variant.InOrder),
                           CheckLine("abc", CheckLine.Variant.DAG),
                           CheckLine("def", CheckLine.Variant.DAG),
                           CheckLine("bar", CheckLine.Variant.InOrder) ],
                         """foo
                            abc
                            bar""")
    self.__notMatchMulti([ CheckLine("foo", CheckLine.Variant.InOrder),
                           CheckLine("abc", CheckLine.Variant.DAG),
                           CheckLine("def", CheckLine.Variant.DAG),
                           CheckLine("bar", CheckLine.Variant.InOrder) ],
                         """foo
                            def
                            bar""")

  def test_NotAssertions(self):
    self.__matchMulti([ CheckLine("foo", CheckLine.Variant.Not) ],
                      """abc
                         def""")
    self.__notMatchMulti([ CheckLine("foo", CheckLine.Variant.Not) ],
                         """abc foo
                            def""")
    self.__notMatchMulti([ CheckLine("foo", CheckLine.Variant.Not),
                           CheckLine("bar", CheckLine.Variant.Not) ],
                         """abc
                            def bar""")

  def test_LineOnlyMatchesOnce(self):
    self.__matchMulti([ CheckLine("foo", CheckLine.Variant.DAG),
                        CheckLine("foo", CheckLine.Variant.DAG) ],
                       """foo
                          foo""")
    self.__notMatchMulti([ CheckLine("foo", CheckLine.Variant.DAG),
                           CheckLine("foo", CheckLine.Variant.DAG) ],
                          """foo
                             bar""")

if __name__ == '__main__':
  Logger.Verbosity = Logger.Level.NoOutput
  unittest.main()

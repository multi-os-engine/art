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

from line   import CheckElement, CheckLine
from logger import Logger

import unittest

CheckerException = SystemExit

class TestCheckLine_Parse(unittest.TestCase):
  def __getPartPattern(self, linePart):
    if linePart.variant == CheckElement.Variant.Separator:
      return "\s+"
    else:
      return linePart.pattern

  def __getRegex(self, checkLine):
    return "".join(map(lambda x: "(" + self.__getPartPattern(x) + ")", checkLine.lineParts))

  def __tryParse(self, string):
    return CheckLine(string)

  def __parsesTo(self, string, expected):
    self.assertEqual(expected, self.__getRegex(self.__tryParse(string)))

  def __tryParseNot(self, string):
    return CheckLine(string, CheckLine.Variant.Not)

  def __parsesPattern(self, string, pattern):
    line = self.__tryParse(string)
    self.assertEqual(1, len(line.lineParts))
    self.assertEqual(CheckElement.Variant.Pattern, line.lineParts[0].variant)
    self.assertEqual(pattern, line.lineParts[0].pattern)

  def __parsesVarRef(self, string, name):
    line = self.__tryParse(string)
    self.assertEqual(1, len(line.lineParts))
    self.assertEqual(CheckElement.Variant.VarRef, line.lineParts[0].variant)
    self.assertEqual(name, line.lineParts[0].name)

  def __parsesVarDef(self, string, name, body):
    line = self.__tryParse(string)
    self.assertEqual(1, len(line.lineParts))
    self.assertEqual(CheckElement.Variant.VarDef, line.lineParts[0].variant)
    self.assertEqual(name, line.lineParts[0].name)
    self.assertEqual(body, line.lineParts[0].pattern)

  def __doesNotParse(self, string, partType):
    line = self.__tryParse(string)
    self.assertEqual(1, len(line.lineParts))
    self.assertNotEqual(partType, line.lineParts[0].variant)

  # Test that individual parts of the line are recognized

  def test_TextOnly(self):
    self.__parsesTo("foo", "(foo)")
    self.__parsesTo("  foo  ", "(foo)")
    self.__parsesTo("f$o^o", "(f\$o\^o)")

  def test_TextWithWhitespace(self):
    self.__parsesTo("foo bar", "(foo)(\s+)(bar)")
    self.__parsesTo("foo   bar", "(foo)(\s+)(bar)")

  def test_RegexOnly(self):
    self.__parsesPattern("{{a?b.c}}", "a?b.c")

  def test_VarRefOnly(self):
    self.__parsesVarRef("[[ABC]]", "ABC")

  def test_VarDefOnly(self):
    self.__parsesVarDef("[[ABC:a?b.c]]", "ABC", "a?b.c")

  def test_TextWithRegex(self):
    self.__parsesTo("foo{{abc}}bar", "(foo)(abc)(bar)")

  def test_TextWithVar(self):
    self.__parsesTo("foo[[ABC:abc]]bar", "(foo)(abc)(bar)")

  def test_PlainWithRegexAndWhitespaces(self):
    self.__parsesTo("foo {{abc}}bar", "(foo)(\s+)(abc)(bar)")
    self.__parsesTo("foo{{abc}} bar", "(foo)(abc)(\s+)(bar)")
    self.__parsesTo("foo {{abc}} bar", "(foo)(\s+)(abc)(\s+)(bar)")

  def test_PlainWithVarAndWhitespaces(self):
    self.__parsesTo("foo [[ABC:abc]]bar", "(foo)(\s+)(abc)(bar)")
    self.__parsesTo("foo[[ABC:abc]] bar", "(foo)(abc)(\s+)(bar)")
    self.__parsesTo("foo [[ABC:abc]] bar", "(foo)(\s+)(abc)(\s+)(bar)")

  def test_AllKinds(self):
    self.__parsesTo("foo [[ABC:abc]]{{def}}bar", "(foo)(\s+)(abc)(def)(bar)")
    self.__parsesTo("foo[[ABC:abc]] {{def}}bar", "(foo)(abc)(\s+)(def)(bar)")
    self.__parsesTo("foo [[ABC:abc]] {{def}} bar", "(foo)(\s+)(abc)(\s+)(def)(\s+)(bar)")

  # Test that variables and patterns are parsed correctly

  def test_ValidPattern(self):
    self.__parsesPattern("{{abc}}", "abc")
    self.__parsesPattern("{{a[b]c}}", "a[b]c")
    self.__parsesPattern("{{(a{bc})}}", "(a{bc})")

  def test_ValidRef(self):
    self.__parsesVarRef("[[ABC]]", "ABC")
    self.__parsesVarRef("[[A1BC2]]", "A1BC2")

  def test_ValidDef(self):
    self.__parsesVarDef("[[ABC:abc]]", "ABC", "abc")
    self.__parsesVarDef("[[ABC:ab:c]]", "ABC", "ab:c")
    self.__parsesVarDef("[[ABC:a[b]c]]", "ABC", "a[b]c")
    self.__parsesVarDef("[[ABC:(a[bc])]]", "ABC", "(a[bc])")

  def test_Empty(self):
    self.__doesNotParse("{{}}", CheckElement.Variant.Pattern)
    self.__doesNotParse("[[]]", CheckElement.Variant.VarRef)
    self.__doesNotParse("[[:]]", CheckElement.Variant.VarDef)

  def test_InvalidVarName(self):
    self.__doesNotParse("[[0ABC]]", CheckElement.Variant.VarRef)
    self.__doesNotParse("[[AB=C]]", CheckElement.Variant.VarRef)
    self.__doesNotParse("[[ABC=]]", CheckElement.Variant.VarRef)
    self.__doesNotParse("[[0ABC:abc]]", CheckElement.Variant.VarDef)
    self.__doesNotParse("[[AB=C:abc]]", CheckElement.Variant.VarDef)
    self.__doesNotParse("[[ABC=:abc]]", CheckElement.Variant.VarDef)

  def test_BodyMatchNotGreedy(self):
    self.__parsesTo("{{abc}}{{def}}", "(abc)(def)")
    self.__parsesTo("[[ABC:abc]][[DEF:def]]", "(abc)(def)")

  def test_NoVarDefsInNotChecks(self):
    with self.assertRaises(CheckerException):
      self.__tryParseNot("[[ABC:abc]]")

class TestCheckLine_Match(unittest.TestCase):
  def __matchSingle(self, checkString, outputString, varState={}):
    checkLine = CheckLine(checkString)
    newVarState = checkLine.match(outputString, varState)
    self.assertIsNotNone(newVarState)
    return newVarState

  def __notMatchSingle(self, checkString, outputString, varState={}):
    checkLine = CheckLine(checkString)
    self.assertIsNone(checkLine.match(outputString, varState))

  def test_TextAndWhitespace(self):
    self.__matchSingle("foo", "foo")
    self.__matchSingle("foo", "  foo  ")
    self.__matchSingle("foo", "foo bar")
    self.__notMatchSingle("foo", "XfooX")
    self.__notMatchSingle("foo", "zoo")

    self.__matchSingle("foo bar", "foo   bar")
    self.__matchSingle("foo bar", "abc foo bar def")
    self.__matchSingle("foo bar", "foo foo bar bar")

    self.__matchSingle("foo bar", "foo X bar")
    self.__notMatchSingle("foo bar", "foo Xbar")

  def test_Pattern(self):
    self.__matchSingle("foo{{A|B}}bar", "fooAbar")
    self.__matchSingle("foo{{A|B}}bar", "fooBbar")
    self.__notMatchSingle("foo{{A|B}}bar", "fooCbar")

  def test_VariableReference(self):
    self.__matchSingle("foo[[X]]bar", "foobar", {"X": ""})
    self.__matchSingle("foo[[X]]bar", "fooAbar", {"X": "A"})
    self.__matchSingle("foo[[X]]bar", "fooBbar", {"X": "B"})
    self.__notMatchSingle("foo[[X]]bar", "foobar", {"X": "A"})
    self.__notMatchSingle("foo[[X]]bar", "foo bar", {"X": "A"})
    with self.assertRaises(CheckerException):
      self.__matchSingle("foo[[X]]bar", "foobar", {})

  def test_VariableDefinition(self):
    self.__matchSingle("foo[[X:A|B]]bar", "fooAbar")
    self.__matchSingle("foo[[X:A|B]]bar", "fooBbar")
    self.__notMatchSingle("foo[[X:A|B]]bar", "fooCbar")

    env = self.__matchSingle("foo[[X:A.*B]]bar", "fooABbar", {})
    self.assertEqual(env, {"X": "AB"})
    env = self.__matchSingle("foo[[X:A.*B]]bar", "fooAxxBbar", {})
    self.assertEqual(env, {"X": "AxxB"})

    self.__matchSingle("foo[[X:A|B]]bar[[X]]baz", "fooAbarAbaz")
    self.__matchSingle("foo[[X:A|B]]bar[[X]]baz", "fooBbarBbaz")
    self.__notMatchSingle("foo[[X:A|B]]bar[[X]]baz", "fooAbarBbaz")

  def test_NoVariableRedefinition(self):
    with self.assertRaises(CheckerException):
      self.__matchSingle("[[X:...]][[X]][[X:...]][[X]]", "foofoobarbar")

  def test_EnvNotChangedOnPartialMatch(self):
    env = {"Y": "foo"}
    self.__notMatchSingle("[[X:A]]bar", "Abaz", env)
    self.assertFalse("X" in env.keys())

  def test_VariableContentEscaped(self):
    self.__matchSingle("[[X:..]]foo[[X]]", ".*foo.*")
    self.__notMatchSingle("[[X:..]]foo[[X]]", ".*fooAAAA")

if __name__ == '__main__':
  Logger.Verbosity = Logger.Level.NoOutput
  unittest.main()

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

# This is a test file which exercises all feautres supported by the domain-specific markup language
# implemented by Checker.

import checker
import io
import unittest


class TestCheckLinePrefix(unittest.TestCase):
  def _tryParse(self, string):
    checkStream = io.StringIO(string)
    checkFile = checker.CheckFile("XYZ", checkStream)
    if len(checkFile.checkLines) == 1:
      return checkFile.checkLines[0]
    else:
      return None

  def test_InvalidFormat(self):
    self.assertIsNone(self._tryParse("XYZ"))
    self.assertIsNone(self._tryParse(":XYZ"))
    self.assertIsNone(self._tryParse("XYZ:"))
    self.assertIsNone(self._tryParse("//XYZ"))
    self.assertIsNone(self._tryParse("#XYZ"))

    self.assertIsNotNone(self._tryParse("//XYZ:foo"))
    self.assertIsNotNone(self._tryParse("#XYZ:bar"))

  def test_InvalidLabel(self):
    self.assertIsNone(self._tryParse("//AXYZ:foo"))
    self.assertIsNone(self._tryParse("#AXYZ:foo"))

  def test_NotFirstOnTheLine(self):
    self.assertIsNone(self._tryParse("A// XYZ: foo"))
    self.assertIsNone(self._tryParse("A # XYZ: foo"))
    self.assertIsNone(self._tryParse("// // XYZ: foo"))
    self.assertIsNone(self._tryParse("# # XYZ: foo"))

  def test_WhitespaceAgnostic(self):
    self.assertIsNotNone(self._tryParse("  //XYZ: foo"))
    self.assertIsNotNone(self._tryParse("//  XYZ: foo"))
    self.assertIsNotNone(self._tryParse("    //XYZ: foo"))
    self.assertIsNotNone(self._tryParse("//    XYZ: foo"))


class TestCheckLineParsing(unittest.TestCase):
  def _getRegex(self, checkLine):
    return "".join(map(lambda x: "(" + x.pattern + ")", checkLine.regexParts))

  def _tryParse(self, string):
    checkStream = io.StringIO("//XYZ:" + string)
    checkFile = checker.CheckFile("XYZ", checkStream)
    if len(checkFile.checkLines) == 1:
      return checkFile.checkLines[0]
    else:
      return None

  def _parsesTo(self, string, expected):
    self.assertEqual(expected, self._getRegex(self._tryParse(string)))

  def _parsesPattern(self, string, pattern):
    line = self._tryParse(string)
    self.assertEqual(1, len(line.regexParts))
    self.assertEqual(checker.RegexVariant.Pattern, line.regexParts[0].variant)
    self.assertEqual(pattern, line.regexParts[0].pattern)

  def _parsesVarRef(self, string, name):
    line = self._tryParse(string)
    self.assertEqual(1, len(line.regexParts))
    self.assertEqual(checker.RegexVariant.VarRef, line.regexParts[0].variant)
    self.assertEqual(name, line.regexParts[0].name)

  def _parsesVarDef(self, string, name, body):
    line = self._tryParse(string)
    self.assertEqual(1, len(line.regexParts))
    self.assertEqual(checker.RegexVariant.VarDef, line.regexParts[0].variant)
    self.assertEqual(name, line.regexParts[0].name)
    self.assertEqual(body, line.regexParts[0].pattern)

  def _doesNotParse(self, string, partType):
    line = self._tryParse(string)
    self.assertEqual(1, len(line.regexParts))
    self.assertNotEqual(partType, line.regexParts[0].variant)

  # Test that individual parts of the line are recognized

  def test_TextOnly(self):
    self._parsesTo("foo", "(foo)")
    self._parsesTo("  foo  ", "(foo)")
    self._parsesTo("f$o^o", "(f\$o\^o)")

  def test_TextWithWhitespace(self):
    self._parsesTo("foo bar", "(foo)(\s+)(bar)")
    self._parsesTo("foo   bar", "(foo)(\s+)(bar)")

  def test_RegexOnly(self):
    self._parsesPattern("{{a?b.c}}", "a?b.c")

  def test_VarRefOnly(self):
    self._parsesVarRef("[[ABC]]", "ABC")

  def test_VarDefOnly(self):
    self._parsesVarDef("[[ABC:a?b.c]]", "ABC", "a?b.c")

  def test_TextWithRegex(self):
    self._parsesTo("foo{{abc}}bar", "(foo)(abc)(bar)")

  def test_TextWithVar(self):
    self._parsesTo("foo[[ABC:abc]]bar", "(foo)(abc)(bar)")

  def test_PlainWithRegexAndWhitespaces(self):
    self._parsesTo("foo {{abc}}bar", "(foo)(\s+)(abc)(bar)")
    self._parsesTo("foo{{abc}} bar", "(foo)(abc)(\s+)(bar)")
    self._parsesTo("foo {{abc}} bar", "(foo)(\s+)(abc)(\s+)(bar)")

  def test_PlainWithVarAndWhitespaces(self):
    self._parsesTo("foo [[ABC:abc]]bar", "(foo)(\s+)(abc)(bar)")
    self._parsesTo("foo[[ABC:abc]] bar", "(foo)(abc)(\s+)(bar)")
    self._parsesTo("foo [[ABC:abc]] bar", "(foo)(\s+)(abc)(\s+)(bar)")

  def test_AllKinds(self):
    self._parsesTo("foo [[ABC:abc]]{{def}}bar", "(foo)(\s+)(abc)(def)(bar)")
    self._parsesTo("foo[[ABC:abc]] {{def}}bar", "(foo)(abc)(\s+)(def)(bar)")
    self._parsesTo("foo [[ABC:abc]] {{def}} bar", "(foo)(\s+)(abc)(\s+)(def)(\s+)(bar)")

  # Test that variables and patterns are parsed correctly

  def test_ValidPattern(self):
    self._parsesPattern("{{abc}}", "abc")
    self._parsesPattern("{{a[b]c}}", "a[b]c")
    self._parsesPattern("{{(a{bc})}}", "(a{bc})")

  def test_ValidRef(self):
    self._parsesVarRef("[[ABC]]", "ABC")
    self._parsesVarRef("[[A1BC2]]", "A1BC2")

  def test_ValidDef(self):
    self._parsesVarDef("[[ABC:abc]]", "ABC", "abc")
    self._parsesVarDef("[[ABC:ab:c]]", "ABC", "ab:c")
    self._parsesVarDef("[[ABC:a[b]c]]", "ABC", "a[b]c")
    self._parsesVarDef("[[ABC:(a[bc])]]", "ABC", "(a[bc])")

  def test_Empty(self):
    self._doesNotParse("{{}}", checker.RegexVariant.Pattern)
    self._doesNotParse("[[]]", checker.RegexVariant.VarRef)
    self._doesNotParse("[[:]]", checker.RegexVariant.VarDef)

  def test_InvalidVarName(self):
    self._doesNotParse("[[0ABC]]", checker.RegexVariant.VarRef)
    self._doesNotParse("[[AB=C]]", checker.RegexVariant.VarRef)
    self._doesNotParse("[[ABC=]]", checker.RegexVariant.VarRef)
    self._doesNotParse("[[0ABC:abc]]", checker.RegexVariant.VarDef)
    self._doesNotParse("[[AB=C:abc]]", checker.RegexVariant.VarDef)
    self._doesNotParse("[[ABC=:abc]]", checker.RegexVariant.VarDef)

  def test_BodyMatchNotGreedy(self):
    self._parsesTo("{{abc}}{{def}}", "(abc)(def)")
    self._parsesTo("[[ABC:abc]][[DEF:def]]", "(abc)(def)")


class TestMatchingCheckLine(unittest.TestCase):
  def _matchSingle(self, checkString, inputString, env={}):
    checkLine = checker.CheckLine(checkString)
    newEnv = checkLine.match(inputString, env)
    self.assertIsNotNone(newEnv)
    return newEnv

  def _notMatchSingle(self, checkString, inputString, env={}):
    checkLine = checker.CheckLine(checkString)
    self.assertIsNone(checkLine.match(inputString, env))

  def _matchMulti(self, checkString, inputString):
    checkStream = io.StringIO(checkString)
    inputStream = io.StringIO(inputString)
    checkFile = checker.CheckFile("XYZ", checkStream)
    return checkFile.match(inputStream)

  def _notMatchMulti(self, checkString, inputString):
    with self.assertRaises(Exception):
      self._matchMulti(checkString, inputString)

  def test_SingleLine_TextAndWhitespace(self):
    self._matchSingle("foo", "foo")
    self._matchSingle("foo", "XfooX")
    self._matchSingle("foo", "foo bar")
    self._notMatchSingle("foo", "zoo")

    self._matchSingle("foo bar", "foo   bar")
    self._matchSingle("foo bar", "abc foo bar def")
    self._matchSingle("foo bar", "foo foo bar bar")
    self._notMatchSingle("foo bar", "foo abc bar")

  def test_SingleLine_Pattern(self):
    self._matchSingle("foo{{A|B}}bar", "fooAbar")
    self._matchSingle("foo{{A|B}}bar", "fooBbar")
    self._notMatchSingle("foo{{A|B}}bar", "fooCbar")

  def test_SingleLine_VarRef(self):
    self._matchSingle("foo[[X]]bar", "foobar", {"X": ""})
    self._matchSingle("foo[[X]]bar", "fooAbar", {"X": "A"})
    self._matchSingle("foo[[X]]bar", "fooBbar", {"X": "B"})
    self._notMatchSingle("foo[[X]]bar", "foobar", {"X": "A"})
    self._notMatchSingle("foo[[X]]bar", "foo bar", {"X": "A"})
    with self.assertRaises(Exception):
      self._matchSingle("foo[[X]]bar", "foobar", {})

  def test_SingleLine_VarDef(self):
    self._matchSingle("foo[[X:A|B]]bar", "fooAbar")
    self._matchSingle("foo[[X:A|B]]bar", "fooBbar")
    self._notMatchSingle("foo[[X:A|B]]bar", "fooCbar")

    env = self._matchSingle("foo[[X:A.*B]]bar", "fooABbar", {})
    self.assertEqual(env, {"X": "AB"})
    env = self._matchSingle("foo[[X:A.*B]]bar", "fooAxxBbar", {})
    self.assertEqual(env, {"X": "AxxB"})

    self._matchSingle("foo[[X:A|B]]bar[[X]]baz", "fooAbarAbaz")
    self._matchSingle("foo[[X:A|B]]bar[[X]]baz", "fooBbarBbaz")
    self._notMatchSingle("foo[[X:A|B]]bar[[X]]baz", "fooAbarBbaz")

    self._matchSingle("[[X:...]][[X]][[X:...]][[X]]", "foofoobarbar")

  def test_MultiLine_TextAndPattern(self):
    self._matchMulti(
      """//XYZ: foo bar
         //XYZ: abc {{def}}""",
      """foo bar
         abc def""");
    self._matchMulti(
      """//XYZ: foo bar
         //XYZ: abc {{de.}}""",
      """=======
         foo bar
         =======
         abc de#
         =======""");
    self._notMatchMulti(
      """//XYZ: foo bar
         //XYZ: abc {{def}}""",
      """=======
         foo bar
         =======
         abc de#
         =======""");

  def test_MultiLine_Variables(self):
    self._matchMulti(
      """//XYZ: foo[[X:.]]bar
         //XYZ: abc[[X]]def""",
      """foo bar
         abc def""");
    self._matchMulti(
      """//XYZ: foo[[X:([0-9]+)]]bar
         //XYZ: abc[[X]]def
         //XYZ: ### [[X]] ###""",
      """foo1234bar
         abc1234def
         ### 1234 ###""");

  def test_EnvNotChangedOnPartialMatch(self):
    env = {"Y": "foo"}
    self._notMatchSingle("[[X:A]]bar", "Abaz", env)
    self.assertFalse("X" in env.keys())

  def test_VariableContentEscaped(self):
    self._matchSingle("[[X:..]]foo[[X]]", ".*foo.*")
    self._notMatchSingle("[[X:..]]foo[[X]]", ".*fooAAAA")

if __name__ == '__main__':
  unittest.main()

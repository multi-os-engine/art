#!/usr/bin/env python3

import check
import io
import unittest


class TestCheckFile(unittest.TestCase):
    def setUp(self):
        self.args = check.ParseArguments("--check-prefix=XYZ Example.java")
        self.checkFile = check.CheckFile()

    def parse(self, string):
        return self.checkFile.parseLine(string, 1, self.args)

    def test_CheckLinePrefix(self):
        self.assertIsNone(self.parse("XYZ"))
        self.assertIsNone(self.parse(":XYZ"))
        self.assertIsNone(self.parse("XYZ:"))
        self.assertIsNone(self.parse("//XYZ"))
        self.assertIsNone(self.parse("#XYZ"))

        self.assertIsNotNone(self.parse("//XYZ:foo"))
        self.assertIsNotNone(self.parse("#XYZ:bar"))

    def test_CheckLinePrefix_WrongLabel(self):
        self.assertIsNone(self.parse("//AXYZ:foo"))
        self.assertIsNone(self.parse("#AXYZ:foo"))

    def test_CheckLinePrefix_Whitespace(self):
        self.assertIsNotNone(self.parse("  //XYZ: foo"))
        self.assertIsNotNone(self.parse("//  XYZ: foo"))
        self.assertIsNotNone(self.parse("        //XYZ: foo"))
        self.assertIsNotNone(self.parse("//        XYZ: foo"))

    def test_CheckLinePrefix_NotAtStart(self):
        self.assertIsNone(self.parse("A// XYZ: foo"))
        self.assertIsNone(self.parse("A # XYZ: foo"))
        self.assertIsNone(self.parse("// // XYZ: foo"))
        self.assertIsNone(self.parse("# # XYZ: foo"))

    def test_CheckLineContent(self):
        self.assertEqual("foo", self.parse("//XYZ:foo").pattern)
        self.assertEqual("bar", self.parse("#XYZ:      bar      ").pattern)


class TestMatchLine(unittest.TestCase):
    def match(self, string, pattern):
        checkLine = check.CheckLine(pattern)
        inputStream = io.StringIO(string)
        return check.MatchLine(inputStream, checkLine)

    def test_SingleInputLine(self):
        self.assertEqual(1, self.match("foo bar", "foo bar"))
        self.assertEqual(1, self.match("foo bar", "foo"))
        self.assertEqual(1, self.match("foo bar", "bar"))
        self.assertEqual(-1, self.match("foo bar", "bar foo"))
        self.assertEqual(-1, self.match("foo bar", "zoo"))

    def test_MultipleInputLines(self):
        self.assertEqual(1, self.match("foo\nbar\n", "foo"))
        self.assertEqual(2, self.match("foo\nbar\n", "bar"))
        self.assertEqual(-1, self.match("foo\nbar\n", "zoo"))

    def test_IsWhitespaceAgnostic(self):
        self.assertEqual(-1, self.match("foobar", "foo bar"))
        self.assertEqual(1, self.match("foo bar", "foo bar"))
        self.assertEqual(1, self.match("foo bar", "foo    bar"))
        self.assertEqual(1, self.match("foo bar", "foo     bar"))

if __name__ == '__main__':
    unittest.main()
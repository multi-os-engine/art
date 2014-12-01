#!/usr/bin/env python3

import argparse
import re
import sys


# This class holds information about individual check lines in the check file.
class CheckLine:
    def __init__(self, line_content, linenum=-1):
        # Split the content of the check line at whitespaces and join them back
        # together to form a regex pattern.
        self.lineNumber = linenum
        self.content = line_content.strip()
        self.pattern = "\s+".join(self.content.split())
        if len(self.pattern) == 0:
            raise Exception("Empty check line regex pattern")


# This class parses the check file and holds the list of check lines.
class CheckFile:
    # Attempts to parse a line from the check file. Returns a new CheckLine
    # object if successful and None otherwise.
    def parseLine(self, line, linenum, args):
        comment_symbols = ["//", "#"]
        ignore_whitespace = "\s*"
        prefix_regex = ignore_whitespace + \
                       "(" + "|".join(comment_symbols) + ")" + \
                       ignore_whitespace + \
                       args.check_prefix + ":"

        # Match the regex against the text line. The 'match' function succeeds
        # iff the pattern is matched at the beginning of the line.
        match = re.match(prefix_regex, line)
        if match is not None:
            return CheckLine(line[match.end():], linenum)
        return None

    # Parses the check file specified in command-line arguments.
    def parseFile(self, args):
        lineno = 0
        with open(args.check_file, "r") as f:
            for line in f:
                lineno += 1
                checkLine = self.parseLine(line, lineno, args)
                if checkLine is not None:
                    self.checkLines.append(checkLine)
            f.close()

    def __init__(self, args=None, extraCheckLines=[]):
        self.checkLines = []
        if args is not None:
            self.parseFile(args)
        self.checkLines.extend(extraCheckLines)


# Reads from the input stream until it finds a line matching the given check
# line. Returns the number of lines read or -1 if EOF was reached.
def MatchLine(inputStream, checkLine):
    linesRead = 0
    for line in inputStream:
        linesRead += 1
        match = re.search(checkLine.pattern, line)
        if match is not None:
            return linesRead
    return -1


# Reads from the input stream and attempts to match all the given check lines.
# Returns None if successful and the first unmatched check line otherwise.
def MatchFiles(inputStream, checkFile):
    for checkLine in checkFile.checkLines:
        res = MatchLine(inputStream, checkLine)
        if res == -1:
            return checkLine
    return None


def ParseArguments(cl=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("check_file")
    parser.add_argument("--input-file", default="-")
    parser.add_argument("--check-prefix", default="CHECK")
    if (cl is None):
        return parser.parse_args()
    else:
        return parser.parse_args(cl.split())


if __name__ == "__main__":
    args = ParseArguments()
    checkFile = CheckFile(args)
    inputStream = sys.stdin if (args.input_file == "-") \
                  else open(args.input_file, "r")

    res = MatchFiles(inputStream, checkFile)
    inputStream.close()

    if res is None:
        print("SUCCESS")
    else:
        print(("Could not match check line " + str(res.lineNumber) + ": " +
               res.content))
        sys.exit(1)

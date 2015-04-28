# Copyright (C) 2015 The Android Open Source Project
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

from common.logger import Logger
from common.mixins import EqualityMixin, PrintableMixin

import re

class CheckerFile(PrintableMixin):
  """ TODO """

  def __init__(self, fileName):
    self.fileName = fileName
    self.cases = []

  def getFileName(self):
    return self.fileName

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.cases == other.cases

class TestCase(PrintableMixin):
  """ TODO """

  def __init__(self, parent, name, startLineNo):
    assert isinstance(parent, CheckerFile)

    self.parent = parent
    self.name = name
    self.assertions = []
    self.startLineNo = startLineNo

    if not self.name:
      Logger.fail("Test case does not have a name", parent.getFileName(), startLineNo)

    self.parent.cases.append(self)

  def getFileName(self):
    return self.parent.getFileName()

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.name == other.name \
       and self.assertions == other.assertions


class TestAssertion(PrintableMixin):
  """ TODO """

  class Variant(object):
    """Supported types of assertions."""
    InOrder, DAG, Not = range(3)

  def __init__(self, parent, variant, orig_text, lineNo):
    assert isinstance(parent, TestCase)

    self.parent = parent
    self.variant = variant
    self.expressions = []
    self.lineNo = lineNo
    self.orig_text = orig_text

    # TODO: Move to matcher
    # if not self.expressions:
    #   Logger.fail("Empty assertion", self.getFileName(), self.lineNo)

    self.parent.assertions.append(self)

  def getFileName(self):
    return self.parent.getFileName()

  def addExpression(self, expression):
    assert isinstance(expression, RegexExpression)
    if self.variant == TestAssertion.Variant.Not:
      if expression.variant == RegexExpression.Variant.VarDef:
        Logger.fail("CHECK-NOT lines cannot define variables", self.getFileName(), self.lineNo)
    self.expressions.append(expression)

  def toRegex(self):
    regex = ""
    for expression in self.expressions:
      if expression.variant == RegexExpression.Variant.Separator:
        regex = regex + ", "
      else:
        regex = regex + "(" + expression.pattern + ")"
    return regex

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.variant == other.variant \
       and self.expressions == other.expressions


class RegexExpression(EqualityMixin, PrintableMixin):
  """ TODO """

  class Variant(object):
    """Supported language constructs."""
    Text, Pattern, VarRef, VarDef, Separator = range(5)

  class Regex(object):
    """ TODO """

    rName = r"([a-zA-Z][a-zA-Z0-9]*)"
    rRegex = r"(.+?)"
    rPatternStartSym = r"(\{\{)"
    rPatternEndSym = r"(\}\})"
    rVariableStartSym = r"(\[\[)"
    rVariableEndSym = r"(\]\])"
    rVariableSeparator = r"(:)"

    regexPattern = rPatternStartSym + rRegex + rPatternEndSym
    regexVariableReference = rVariableStartSym + rName + rVariableEndSym
    regexVariableDefinition = rVariableStartSym + rName + rVariableSeparator + rRegex + rVariableEndSym

  def __init__(self, variant, name, pattern):
    self.variant = variant
    self.name = name
    self.pattern = pattern

  def __eq__(self, other):
    return isinstance(other, self.__class__) \
       and self.variant == other.variant \
       and self.name == other.name \
       and self.pattern == other.pattern

  @staticmethod
  def createSeparator():
    return RegexExpression(RegexExpression.Variant.Separator, None, None)

  @staticmethod
  def createText(text):
    return RegexExpression(RegexExpression.Variant.Text, None, re.escape(text))

  @staticmethod
  def createPattern(pattern):
    return RegexExpression(RegexExpression.Variant.Pattern, None, pattern)

  @staticmethod
  def createVariableReference(name):
    assert re.match(RegexExpression.Regex.rName, name)
    return RegexExpression(RegexExpression.Variant.VarRef, name, None)

  @staticmethod
  def createVariableDefinition(name, pattern):
    assert re.match(RegexExpression.Regex.rName, name)
    return RegexExpression(RegexExpression.Variant.VarDef, name, pattern)

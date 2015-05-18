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

from common.logger              import Logger
from file_format.checker.struct import RegexExpression

import re

def nextCheckerWord(expressions):
  """ Splits list of RegexExpressions at the first separator. """
  assert expressions
  word = []
  while expressions:
    head, expressions = expressions[0], expressions[1:]
    if head.variant == RegexExpression.Variant.Separator:
      return word, expressions
    else:
      word.append(head)
  return word, None

def nextStringWord(string):
  """ Splits string at the first whitespace character. """
  assert string
  whitespace = re.search("\s", string)
  if whitespace:
    return string[:whitespace.start()], string[whitespace.end():].strip()
  else:
    return string, None

def matchWords(checkerWord, strWord, varState, pos):
  """ Attempts to match a list of RegexExpressions against a string. 
      Returns updated variable state if successful and None otherwise.
  """
  # Work on a shadow variable state (dictionaries are passed by reference).
  varState = dict(varState)

  for expr in checkerWord:
    # If `expr` is variable reference, replace it with the variable's value.
    if expr.variant == RegexExpression.Variant.VarRef:
      if expr.name in varState:
        pattern = re.escape(varState[expr.name])
      else:
        Logger.testFailed("Multiple definitions of variable \"{}\"".format(expr.name), 
                          pos.fileName, pos.lineNo)
    else:
      pattern = expr.pattern

    # Match the expression's regex pattern against the remainder of the word.
    # Note: re.match will succeed only if matched from the beginning.
    match = re.match(pattern, strWord)
    if not match:
      return None

    # If `expr` was variable definition, update the variable's value.
    if expr.variant == RegexExpression.Variant.VarDef:
      if expr.name not in varState:
        varState[expr.name] = strWord[:match.end()]
      else:
        Logger.testFailed("Multiple definitions of variable \"{}\"".format(expr.name), 
                          pos.fileName, pos.lineNo)

    # Move cursor by deleting the matched characters.
    strWord = strWord[match.end():]

  # Make sure the entire word matched, i.e. `strWord` is empty.
  if strWord:
    return None
  
  return varState

def MatchLines(checkerLine, strLine, varState):
  """ Attempts to match a CHECK line against a string. Returns variable state
      after the match if successful and None otherwise.
  """
  expressions = checkerLine.expressions
  while expressions:
    # Get the next run of RegexExpressions which must match one string word.
    checkerWord, expressions = nextCheckerWord(expressions)

    # Keep reading words from the `strLine` until a match is found.
    wordMatched = False
    while strLine:
      strWord, strLine = nextStringWord(strLine)
      newVarState = matchWords(checkerWord, strWord, varState, checkerLine)
      if newVarState is not None:
        wordMatched = True
        varState = newVarState
        break
    if not wordMatched:
      return None

  # All RegexExpressions matched. Return new variable state.
  return varState

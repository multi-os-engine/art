#!/usr/bin/python3
#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Generate Smali test files for test 957.
"""

from functools import total_ordering
from pathlib import Path
import itertools
import os
import string
import sys

# The maximum number of leaf interfaces to have per object. Increasing this
# increases the number of generated files significantly. This value was chosen
# as it is fairly quick to run and very comprehensive, checking every possible
# interface tree up to 5 layers deep.
MAX_LEAF_IFACE_PER_OBJECT = 4

# The max depth the tree can have given number of leaf nodes. Includes the class
# object in the tree.
MAX_IFACE_DEPTH = MAX_LEAF_IFACE_PER_OBJECT + 1

BUILD_TOP = os.getenv("ANDROID_BUILD_TOP")
if BUILD_TOP is None:
  print("ANDROID_BUILD_TOP not set. Please run build/envsetup.sh", file=sys.stderr)
  sys.exit(1)

COPYRIGHT_HEADER = "".join(map(lambda a: "# " + a, (Path(BUILD_TOP)/"development"/"docs"/"copyright-templates"/"java.txt").open().readlines()))

def GetCopyright():
  return COPYRIGHT_HEADER;

class MainClass:
  """
  A Main.smali file containing the Main class and the main function. It will run
  all the test functions we have.
  """
  MAIN_CLASS_TEMPLATE = """{copyright}

.class public LMain;
.super Ljava/lang/Object;

# class Main {{

.method public constructor <init>()V
    .registers 1
    invoke-direct {{p0}}, Ljava/lang/Object;-><init>()V
    return-void
.end method

{test_groups}

{main_func}

# }}
"""

  MAIN_FUNCTION_TEMPLATE = """
#   public static void main(String[] args) {{
.method public static main([Ljava/lang/String;)V
    .locals 2
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;

    {test_group_invoke}

    return-void
.end method
#   }}
"""

  TEST_GROUP_INVOKE_TEMPLATE = """
#     {test_name}();
    invoke-static {{}}, {test_name}()V
"""

  def __init__(self):
    self.tests = set()

  def add_test(self, ty):
    """
    Add a test for the concrete type 'ty'
    """
    self.tests.add(Func(ty))

  def dump(self, smali_dir):
    """
    Dump this class to its file in smali_dir
    """
    out_file = smali_dir / "Main.smali"
    if out_file.exists():
      out_file.unlink()
    out = out_file.open('w')
    print(str(self), file=out)

  def __str__(self):
    all_tests = sorted(self.tests)
    test_invoke = ""
    test_groups = ""
    for t in all_tests:
      test_groups += str(t)
    for t in all_tests:
      test_invoke += self.TEST_GROUP_INVOKE_TEMPLATE.format(test_name=t.get_name())
    main_func = self.MAIN_FUNCTION_TEMPLATE.format(test_group_invoke=test_invoke)

    return self.MAIN_CLASS_TEMPLATE.format(copyright = GetCopyright(),
                                           test_groups = test_groups,
                                           main_func = main_func)

@total_ordering
class Func:
  """
  A function that tests the functionality of a concrete type. Should only be
  constructed by MainClass.add_test.
  """
  TEST_FUNCTION_TEMPLATE = """
#   public static void {fname}() {{
#     try {{
#       {farg} v = new {farg}();
#       System.out.printf("%s calls default method on %s\\n",
#                         v.CalledClassName(),
#                         v.CalledInterfaceName());
#       return;
#     }} catch (Error e) {{
#       e.printStackTrace(System.out);
#       return;
#     }}
#   }}
.method public static {fname}()V
    .locals 7
    :call_{fname}_try_start
      new-instance v6, L{farg};
      invoke-direct {{v6}}, L{farg};-><init>()V

      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      invoke-virtual {{v6}}, L{farg};->CalledClassName()Ljava/lang/String;
      move-result-object v4
      aput-object v4,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s calls default method on %s\\n"

      invoke-virtual {{v6}}, L{farg};->CalledInterfaceName()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {{v2,v3,v1}}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_{fname}_try_end
    .catch Ljava/lang/Error; {{:call_{fname}_try_start .. :call_{fname}_try_end}} :error_{fname}_start
    :error_{fname}_start
      move-exception v3
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      invoke-virtual {{v3,v2}}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method
"""

  def __init__(self, farg):
    self.farg = farg

  def __lt__(self, o):
    if len(self.get_name()) != len(o.get_name()):
      return len(self.get_name()) < len(o.get_name())
    else:
      return self.get_name() < o.get_name()
  def __eq__(self, o):
    return self.get_name() == o.get_name()

  def __str__(self):
    return self.TEST_FUNCTION_TEMPLATE.format(fname=self.get_name(), farg=self.farg)

  def get_name(self):
    return "TEST_FUNC_{}".format(self.farg)

  def __eq__(self, o):
    return o.farg == self.farg

  def __hash__(self):
    return hash(self.farg)


# An iterator which yields strings made from lowercase letters. First yields
# all 1 length strings, then all 2 and so on.
NAME_GEN = itertools.chain.from_iterable(
    map(lambda n: itertools.product(string.ascii_lowercase, repeat=n),
        itertools.count(1)))

def gensym():
  """
  Returns a globally unique identifier name that is a valid Java symbol.
  """
  return ''.join(next(NAME_GEN))

class TestClass:
  """
  A class that will be instantiated to test default method resolution order.
  """
  TEST_CLASS_TEMPLATE = """{copyright}

.class public L{class_name};
.super Ljava/lang/Object;
.implements L{iface_name};

# public class {class_name} implements {iface_name} {{
#   public String CalledClassName() {{
#     return "[{class_name} {iface_tree}]";
#   }}
# }}

.method public constructor <init>()V
  .registers 1
  invoke-direct {{p0}}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "[{class_name} {iface_tree}]"
  return-object v0
.end method
"""
  def __init__(self, iface):
    self.iface = iface
    self.class_name = "CLASS_"+gensym()

  def get_name(self):
    return self.class_name

  def dump(self, smali_dir):
    out_file = smali_dir / (self.get_name() + ".smali")
    if out_file.exists():
      out_file.unlink()
    out = out_file.open('w')
    print(str(self), file=out)

  def __str__(self):
    return self.TEST_CLASS_TEMPLATE.format(copyright = GetCopyright(),
                                           iface_name = self.iface.get_name(),
                                           iface_tree = dump_tree(self.iface),
                                           class_name = self.class_name)

def dump_tree(*ifaces):
  out = ""
  for i in ifaces:
    out += " ["+ i.get_name() + " " + dump_tree(*i.ifaces) + "]"
  return out.strip()

@total_ordering
class TestInterface:
  """
  An interface that will be used to test default method resolution order.
  """
  TEST_INTERFACE_TEMPLATE = """{copyright}
.class public abstract interface L{class_name};
.super Ljava/lang/Object;
{implements_spec}

# public interface {class_name} {extends} {ifaces} {{
#   public String CalledClassName();

.method public abstract CalledClassName()Ljava/lang/String;
.end method

{funcs}

# }}
"""

  DEFAULT_FUNC_TEMPLATE = """
#   public default String CalledInterfaceName() {{
#     return "[{class_name} {iface_tree}]";
#   }}
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "[{class_name} {iface_tree}]"
  return-object v0
.end method
"""
  IMPLEMENTS_TEMPLATE = """
.implements L{iface_name};
"""
  def __init__(self, ifaces, default):
    self.ifaces = sorted(ifaces)
    self.default = default
    end = "_DEFAULT" if default else ""
    self.class_name = "INTERFACE_"+gensym()+end

  def get_name(self):
    return self.class_name

  def __iter__(self):
    """
    Performs depth-first traversal of the interface tree this interface is the
    root of. Does not filter out repeats.
    """
    for i in self.ifaces:
      yield i
      yield from i

  def dump(self, smali_dir):
    out_file = smali_dir / (self.get_name() + ".smali")
    if out_file.exists():
      out_file.unlink()
    out = out_file.open('w')
    print(str(self), file=out)

  def __lt__(self, o):
    return self.get_name() < o.get_name()
  def __eq__(self, o):
    return self.get_name() == o.get_name()
  def __hash__(self):
    return hash(self.get_name())
  def __str__(self):
    s_ifaces = " "
    j_ifaces = " "
    for i in self.ifaces:
      s_ifaces += self.IMPLEMENTS_TEMPLATE.format(iface_name = i.get_name())
      j_ifaces += " {},".format(i.get_name())
    j_ifaces = j_ifaces[0:-1]
    if self.default:
      funcs = self.DEFAULT_FUNC_TEMPLATE.format(ifaces = j_ifaces,
                                                iface_tree = dump_tree(*self.ifaces),
                                                class_name = self.class_name)
    else:
      funcs = ""
    return self.TEST_INTERFACE_TEMPLATE.format(copyright = GetCopyright(),
                                               implements_spec = s_ifaces,
                                               extends = "extends" if len(self.ifaces) else "",
                                               ifaces = j_ifaces,
                                               funcs = funcs,
                                               iface_tree = dump_tree(*self.ifaces),
                                               class_name = self.class_name)

def SubtreeSizes(n):
  """
  A generator that yields a tuple containing a possible arrangement of subtree
  nodes for a tree with a total of 'n' leaf nodes.
  """
  if n == 0:
    return
  elif n == 1:
    yield []
  elif n == 2:
    yield [1, 1]
  else:
    for prev in SubtreeSizes(n - 1):
      yield [1] + prev
      for i in range(len(prev)):
        prev[i] += 1
        yield prev
        prev[i] -= 1

# The deduplicated output of SubtreeSizes for each size up to
# MAX_LEAF_IFACE_PER_OBJECT.
SUBTREES = [set(map(lambda l: tuple(sorted(l)), SubtreeSizes(i)))
            for i in range(MAX_IFACE_DEPTH + 1)]

def CreateInterfaceTrees():
  def dump_supers(s):
    """
    Does depth first traversal of all the interfaces in the list.
    """
    for i in s:
      yield i
      yield from i

  def CreateInterfaceTreesInner(num, allow_default):
    for split in SUBTREES[num]:
      ifaces = []
      for sub in split:
        if sub == 1:
          ifaces.append([TestInterface([], allow_default)])
          if allow_default:
            ifaces[-1].append(TestInterface([], False))
        else:
          ifaces.append(list(CreateInterfaceTreesInner(sub, allow_default)))
      for supers in itertools.product(*ifaces):
        all_supers = sorted(set(dump_supers(supers)) - set(supers))
        for i in range(len(all_supers) + 1):
          for combo in itertools.combinations(all_supers, i):
            yield TestInterface(list(combo) + list(supers), allow_default)
      if allow_default:
        for i in range(len(split)):
          ifaces = []
          for sub, cs in zip(split, itertools.count()):
            if sub == 1:
              ifaces.append([TestInterface([], i == cs)])
            else:
              ifaces.append(list(CreateInterfaceTreesInner(sub, i == cs)))
          for supers in itertools.product(*ifaces):
            all_supers = sorted(set(dump_supers(supers)) - set(supers))
            for i in range(len(all_supers) + 1):
              for combo in itertools.combinations(all_supers, i):
                yield TestInterface(list(combo) + list(supers), False)

  for num in range(1, MAX_IFACE_DEPTH):
    yield from CreateInterfaceTreesInner(num, True)

def CreateAllTestFiles():
  """
  Creates all the objects representing the files in this test. They just need to
  be dumped.
  """
  mc = MainClass()
  classes = {mc}
  for tree in CreateInterfaceTrees():
    classes.add(tree)
    for i in tree:
      classes.add(i)
    test_class = TestClass(tree)
    mc.add_test(test_class.get_name())
    classes.add(test_class)
  return classes

def main(argv):
  smali_dir = Path(argv[1])
  if not smali_dir.exists() or not smali_dir.is_dir():
    print("{} is not a valid smali dir".format(smali_dir), file=sys.stderr)
    sys.exit(1)
  for f in CreateAllTestFiles():
    f.dump(smali_dir)

if __name__ == '__main__':
  main(sys.argv)

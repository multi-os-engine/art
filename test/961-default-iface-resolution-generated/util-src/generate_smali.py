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
Generate Smali test files for test 961.
"""

import os
import sys
from pathlib import Path

BUILD_TOP = os.getenv("ANDROID_BUILD_TOP")
if BUILD_TOP is None:
  print("ANDROID_BUILD_TOP not set. Please run build/envsetup.sh", file=sys.stderr)
  sys.exit(1)

sys.path.append(str(Path(BUILD_TOP)/"art"/"test"/"utils"/"python"))

from utils import get_copyright, subtree_sizes, gensym, filter_blanks

from functools import total_ordering
import itertools
import string

# The max depth the type tree can have. Includes the class object in the tree.
# Increasing this increases the number of generated files significantly. This
# value was chosen as it is fairly quick to run and very comprehensive, checking
# every possible interface tree up to 5 layers deep.
MAX_IFACE_DEPTH = 5

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

  def get_expected(self):
    all_tests = sorted(self.tests)
    return filter_blanks("\n".join(map(lambda a: a.get_expected(), all_tests)))

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

    return self.MAIN_CLASS_TEMPLATE.format(copyright = get_copyright("smali"),
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

  def get_expected(self):
    return "{tree} calls default method on {iface_tree}".format(
        tree = self.farg.get_tree(), iface_tree = self.farg.get_called().get_tree())

  def __lt__(self, o):
    if len(self.get_name()) != len(o.get_name()):
      return len(self.get_name()) < len(o.get_name())
    else:
      return self.get_name() < o.get_name()

  def __eq__(self, o):
    return o.farg == self.farg

  def __str__(self):
    return self.TEST_FUNCTION_TEMPLATE.format(fname=self.get_name(), farg=self.farg.get_name())

  def get_name(self):
    return "TEST_FUNC_{}".format(self.farg.get_name())

  def __hash__(self):
    return hash(self.farg.get_name())

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
#     return "{tree}";
#   }}
# }}

.method public constructor <init>()V
  .registers 1
  invoke-direct {{p0}}, Ljava/lang/Object;-><init>()V
  return-void
.end method

.method public CalledClassName()Ljava/lang/String;
  .locals 1
  const-string v0, "{tree}"
  return-object v0
.end method
"""
  def __init__(self, iface):
    self.iface = iface
    self.class_name = "CLASS_"+gensym()

  def get_name(self):
    return self.class_name

  def get_tree(self):
    return "[{class_name} {iface_tree}]".format(class_name = self.class_name,
                                                iface_tree = self.iface.get_tree())

  def __iter__(self):
    yield self.iface
    yield from self.iface

  def get_called(self):
    all_ifaces = set(iface for iface in self if iface.default)
    for i in all_ifaces:
      if all(map(lambda j: i not in j.get_super_types(), all_ifaces)):
        return i
    raise Exception("UNREACHABLE! Unable to find default method!")

  def dump(self, smali_dir):
    out_file = smali_dir / (self.get_name() + ".smali")
    if out_file.exists():
      out_file.unlink()
    out = out_file.open('w')
    print(str(self), file=out)

  def __str__(self):
    return self.TEST_CLASS_TEMPLATE.format(copyright = get_copyright('smali'),
                                           iface_name = self.iface.get_name(),
                                           tree = self.get_tree(),
                                           class_name = self.class_name)

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
#     return "{tree}";
#   }}
.method public CalledInterfaceName()Ljava/lang/String;
  .locals 1
  const-string v0, "{tree}"
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

  def get_super_types(self):
    return set(i2 for i2 in self)

  def get_name(self):
    return self.class_name

  def get_tree(self):
    return "[{class_name} {iftree}]".format(class_name = self.get_name(),
                                            iftree = print_tree(self.ifaces))

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
                                                tree = self.get_tree(),
                                                class_name = self.class_name)
    else:
      funcs = ""
    return self.TEST_INTERFACE_TEMPLATE.format(copyright = get_copyright('smali'),
                                               implements_spec = s_ifaces,
                                               extends = "extends" if len(self.ifaces) else "",
                                               ifaces = j_ifaces,
                                               funcs = funcs,
                                               tree = self.get_tree(),
                                               class_name = self.class_name)

def print_tree(ifaces):
  return " ".join(i.get_tree() for i in  ifaces)

# The deduplicated output of subtree_sizes for each size up to
# MAX_LEAF_IFACE_PER_OBJECT.
SUBTREES = [set(map(lambda l: tuple(sorted(l)), subtree_sizes(i)))
            for i in range(MAX_IFACE_DEPTH + 1)]

def create_interface_trees():
  def dump_supers(s):
    """
    Does depth first traversal of all the interfaces in the list.
    """
    for i in s:
      yield i
      yield from i

  def create_interface_trees_inner(num, allow_default):
    for split in SUBTREES[num]:
      ifaces = []
      for sub in split:
        if sub == 1:
          ifaces.append([TestInterface([], allow_default)])
          if allow_default:
            ifaces[-1].append(TestInterface([], False))
        else:
          ifaces.append(list(create_interface_trees_inner(sub, allow_default)))
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
              ifaces.append(list(create_interface_trees_inner(sub, i == cs)))
          for supers in itertools.product(*ifaces):
            all_supers = sorted(set(dump_supers(supers)) - set(supers))
            for i in range(len(all_supers) + 1):
              for combo in itertools.combinations(all_supers, i):
                yield TestInterface(list(combo) + list(supers), False)

  for num in range(1, MAX_IFACE_DEPTH):
    yield from create_interface_trees_inner(num, True)

def create_all_test_files():
  """
  Creates all the objects representing the files in this test. They just need to
  be dumped.
  """
  mc = MainClass()
  classes = {mc}
  for tree in create_interface_trees():
    classes.add(tree)
    for i in tree:
      classes.add(i)
    test_class = TestClass(tree)
    mc.add_test(test_class)
    classes.add(test_class)
  return mc, classes

def main(argv):
  smali_dir = Path(argv[1])
  if not smali_dir.exists() or not smali_dir.is_dir():
    print("{} is not a valid smali dir".format(smali_dir), file=sys.stderr)
    sys.exit(1)
  expected_txt = Path(argv[2])
  mainclass, all_files = create_all_test_files()
  with expected_txt.open('w') as out:
    print(mainclass.get_expected(), file=out)
  for f in all_files:
    f.dump(smali_dir)

if __name__ == '__main__':
  main(sys.argv)

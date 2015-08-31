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
Generate Smali test files for test 964.
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
# interface tree up to 4 layers deep.
MAX_LEAF_IFACE_PER_OBJECT = 2

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
#       System.out.println("About to initialize [{farg} {tree}]");
#       {farg} v = new {farg}();
#       System.out.println("Initialized [{farg} {tree}]");
#       v.touchAll();
#       System.out.println("All of [{farg} {tree}] hierarchy initialized");
#       return;
#     }} catch (Error e) {{
#       e.printStackTrace(System.out);
#       return;
#     }}
#   }}
.method public static {fname}()V
    .locals 7
    :call_{fname}_try_start
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "About to initialize [{farg} {tree}]"
      invoke-virtual {{v2, v3}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

      new-instance v6, L{farg};
      invoke-direct {{v6}}, L{farg};-><init>()V

      const-string v3, "Initialized [{farg} {tree}]"
      invoke-virtual {{v2, v3}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

      invoke-virtual {{v6}}, L{farg};->touchAll()V

      const-string v3, "All of [{farg} {tree}] hierarchy initialized"
      invoke-virtual {{v2, v3}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

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
    return o.farg == self.farg

  def __str__(self):
    return self.TEST_FUNCTION_TEMPLATE.format(fname=self.get_name(),
                                              farg=self.farg.get_name(),
                                              tree = self.farg.get_tree())

  def get_name(self):
    return "TEST_FUNC_{}".format(self.farg.get_name())

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
{implements_spec}

# public class {class_name} implements {ifaces} {{
#
#   public {class_name}() {{
#   }}
.method public constructor <init>()V
  .locals 2
  invoke-direct {{p0}}, Ljava/lang/Object;-><init>()V
  return-void
.end method

#   public void marker() {{
#     return;
#   }}
.method public marker()V
  .locals 0
  return-void
.end method

#   public void touchAll() {{
.method public touchAll()V
  .locals 2
  sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
  {touch_calls}
  return-void
.end method
#   }}
# }}
"""

  IMPLEMENTS_TEMPLATE = """
.implements L{iface_name};
"""

  TOUCH_CALL_TEMPLATE = """
#     System.out.println("{class_name} touching {iface_name}");
#     {iface_name}.field.touch();
      const-string v1, "{class_name} touching {iface_name}"
      invoke-virtual {{v0, v1}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
      sget-object v1, L{iface_name};->field:LDisplayer;
      invoke-virtual {{v1}}, LDisplayer;->touch()V
"""
  def __init__(self, ifaces):
    self.ifaces = ifaces
    self.class_name = "CLASS_"+gensym()

  def get_name(self):
    return self.class_name

  def get_tree(self):
    return print_tree(self.ifaces)

  def dump(self, smali_dir):
    out_file = smali_dir / (self.get_name() + ".smali")
    if out_file.exists():
      out_file.unlink()
    out = out_file.open('w')
    print(str(self), file=out)

  def get_all_interfaces(self):
    return sorted(set(dump_tree(self.ifaces)))

  def __str__(self):
    s_ifaces = '\n'.join(map(lambda a: self.IMPLEMENTS_TEMPLATE.format(iface_name = a.get_name()),
                             self.ifaces))
    j_ifaces = ', '.join(map(lambda a: a.get_name(), self.ifaces))
    touches  = '\n'.join(map(lambda a: self.TOUCH_CALL_TEMPLATE.format(class_name = self.class_name,
                                                                       iface_name = a.get_name()),
                             self.get_all_interfaces()))
    return self.TEST_CLASS_TEMPLATE.format(copyright = GetCopyright(),
                                           tree = self.get_tree(),
                                           implements_spec = s_ifaces,
                                           ifaces = j_ifaces,
                                           class_name = self.class_name,
                                           touch_calls = touches)

def dump_tree(ifaces):
  for i in ifaces:
    yield i
    yield from dump_tree(i.ifaces)

def print_tree(ifaces):
  out = ""
  for i in ifaces:
    out += " ["+ i.get_name() + " " + print_tree(i.ifaces) + "]"
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
#   public static final Displayer field = new Displayer("[{class_name} {tree}]");
#   public void marker();
#
.field public final static field:LDisplayer;

.method public static constructor <clinit>()V
    .locals 3
    const-string v2, "[{class_name} {tree}]"
    new-instance v1, LDisplayer;
    invoke-direct {{v1, v2}}, LDisplayer;-><init>(Ljava/lang/String;)V
    sput-object v1, L{class_name};->field:LDisplayer;
    return-void
.end method


.method public abstract marker()V
.end method

{funcs}

# }}
"""

  DEFAULT_FUNC_TEMPLATE = """
#   public default void {class_name}_DEFAULT_FUNC() {{
#     return;
#   }}
.method public {class_name}_DEFAULT_FUNC()V
  .locals 0
  return-void
.end method
"""
  IMPLEMENTS_TEMPLATE = """
.implements L{iface_name};
"""
  def __init__(self, ifaces, default):
    self.ifaces = ifaces
    self.default = default
    end = "_DEFAULT" if default else ""
    self.class_name = "INTERFACE_"+gensym()+end
    self.cloned = False

  def clone(self):
    return TestInterface(tuple(map(lambda a: a.clone(), self.ifaces)), self.default)

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
  def get_tree(self):
    return print_tree(self.ifaces)

  def __str__(self):
    s_ifaces = '\n'.join(map(lambda a: self.IMPLEMENTS_TEMPLATE.format(iface_name = a.get_name()),
                             self.ifaces))
    j_ifaces = ', '.join(map(lambda a: a.get_name(), self.ifaces))
    if self.default:
      funcs = self.DEFAULT_FUNC_TEMPLATE.format(class_name = self.class_name)
    else:
      funcs = ""
    return self.TEST_INTERFACE_TEMPLATE.format(copyright = GetCopyright(),
                                               implements_spec = s_ifaces,
                                               extends = "extends" if len(self.ifaces) else "",
                                               ifaces = j_ifaces,
                                               funcs = funcs,
                                               tree = self.get_tree(),
                                               class_name = self.class_name)

def SubtreeSizes(n):
  """
  A generator that yields a tuple containing a possible arrangement of subtree
  nodes for a tree with a total of 'n' leaf nodes.
  """
  if n == 0:
    return
  elif n == 1:
    yield [0]
  elif n == 2:
    yield [1, 1]
  else:
    for prev in SubtreeSizes(n - 1):
      yield [1] + prev
      for i in range(len(prev)):
        prev[i] += 1
        yield prev
        prev[i] -= 1

SUBTREES = [set(map(lambda l: tuple(l), SubtreeSizes(i)))
            for i in range(MAX_IFACE_DEPTH + 1)]

def CloneAll(l):
  return tuple(map(lambda a: a.clone(), l))

def CreateTestClasses():
  for num in range(1, MAX_IFACE_DEPTH + 1):
    for split in SUBTREES[num]:
      ifaces = []
      for sub in split:
        ifaces.append(list(CreateInterfaceTrees(sub)))
    for supers in itertools.product(*ifaces):
      yield TestClass(CloneAll(supers))
      for i in range(len(set(dump_tree(supers)) - set(supers))):
        ns = CloneAll(supers)
        selected = sorted(set(dump_tree(ns)) - set(ns))[i]
        yield TestClass(tuple([selected] + list(ns)))

def CreateInterfaceTrees(num):
  if num == 0:
    yield TestInterface(tuple(), False)
    yield TestInterface(tuple(), True)
    return
  for split in SUBTREES[num]:
    ifaces = []
    for sub in split:
      ifaces.append(list(CreateInterfaceTrees(sub)))
    for supers in itertools.product(*ifaces):
      yield TestInterface(CloneAll(supers), False)
      yield TestInterface(CloneAll(supers), True)
      # TODO Should add on some from higher up the tree.

  # for split in SUBTREES[num]:
  #   ifaces = []
  #   for sub in split:
  #     if sub == 1:
  #       ifaces.append([TestInterface([], allow_default)])
  #       if allow_default:
  #         ifaces[-1].append(TestInterface([], False))
  #     else:
  #       ifaces.append(list(CreateInterfaceTreesInner(sub, allow_default)))
  #   for supers in itertools.product(*ifaces):
  #     all_supers = sorted(set(dump_tree(supers)) - set(supers))
  #     for i in range(len(all_supers) + 1):
  #       for combo in itertools.combinations(all_supers, i):
  #         yield TestInterface(list(combo) + list(supers), allow_default)
  #   if allow_default:
  #     for i in range(len(split)):
  #       ifaces = []
  #       for sub, cs in zip(split, itertools.count()):
  #         if sub == 1:
  #           ifaces.append([TestInterface([], i == cs)])
  #         else:
  #           ifaces.append(list(CreateInterfaceTreesInner(sub, i == cs)))
  #       for supers in itertools.product(*ifaces):
  #         all_supers = sorted(set(dump_tree(supers)) - set(supers))
  #         for i in range(len(all_supers) + 1):
  #           for combo in itertools.combinations(all_supers, i):
  #             yield TestInterface(list(combo) + list(supers), False)

  # for num in range(1, MAX_IFACE_DEPTH):
  #   yield from CreateInterfaceTreesInner(num)

def CreateAllTestFiles():
  """
  Creates all the objects representing the files in this test. They just need to
  be dumped.
  """
  mc = MainClass()
  classes = {mc}
  for clazz in CreateTestClasses():
    classes.add(clazz)
    for i in dump_tree(clazz.ifaces):
      classes.add(i)
    mc.add_test(clazz)
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

#!/usr/bin/env python3
"""
Generate Smali test data.
"""

import sys
from pathlib import Path
import itertools
import functools
import string

MAX_BASE_IFACE_PER_OBJECT = 4
CLASS_NAME_LENGTH = 3

@functools.total_ordering
class Func:
  TEST_FUNCTION_TEMPLATE = """
#   public static void {fname}() {{
#     {farg} v = new {farg}();
#     System.out.printf("%s calls default method on %s\\n",
#                       v.CalledClassName(),
#                       v.CalledInterfaceName());
#     return;
#   }}
.method public static {fname}()V
    .locals 7
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
.end method
"""

  def __init__(self, farg):
    self.farg = farg

  def __lt__(self, o):
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


class MainClass:
  MAIN_CLASS_TEMPLATE = """
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

    return self.MAIN_CLASS_TEMPLATE.format(test_groups=test_groups,
                                           main_func=main_func)

NAME_GEN = itertools.product(string.ascii_lowercase, repeat=CLASS_NAME_LENGTH)
def gensym():
  return ''.join(next(NAME_GEN))

class TestClass:
  TEST_CLASS_TEMPLATE = """
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
    return self.TEST_CLASS_TEMPLATE.format(iface_name = self.iface.get_name(),
                                           iface_tree = dump_tree(self.iface),
                                           class_name = self.class_name)

def dump_tree(*ifaces):
  out = ""
  for i in ifaces:
    out += " ["+ i.get_name() + " " + dump_tree(*i.ifaces) + "]"
  return out.strip()

@functools.total_ordering
class TestInterface:
  TEST_INTERFACE_TEMPLATE = """
.class public abstract interface L{class_name};
.super Ljava/lang/Object;
{implements_spec}

# public interface {class_name} {extends} {ifaces} {{

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
    self.ifaces = ifaces
    self.default = default
    end = "_DEFAULT" if default else ""
    self.class_name = "INTERFACE_"+gensym()+end

  def get_name(self):
    return self.class_name

  def __iter__(self):
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
    return self.TEST_INTERFACE_TEMPLATE.format(implements_spec = s_ifaces,
                                               extends = "extends" if len(self.ifaces) else "",
                                               ifaces = j_ifaces,
                                               funcs = funcs,
                                               iface_tree = dump_tree(*self.ifaces),
                                               class_name = self.class_name)

def GetNumberSplits(n):
  if n == 0:
    return
  elif n == 1:
    yield []
  elif n == 2:
    yield [1, 1]
  else:
    for prev in GetNumberSplits(n - 1):
      yield [1] + prev
      for i in range(len(prev)):
        prev[i] += 1
        yield prev
        prev[i] -= 1

NSPLITS = [set(map(lambda l: tuple(sorted(l)), GetNumberSplits(i))) for i in range(0, MAX_BASE_IFACE_PER_OBJECT + 1)]

def CreateInterfaceTrees():
  def dump_supers(s):
    for i in s:
      yield i
      yield from i

  def CreateInterfaceTreesInner(num, allow_default):
    for split in NSPLITS[num]:
      ifaces = []
      for sub in split:
        if sub == 1:
          ifaces.append([TestInterface([], False)])
        else:
          ifaces.append(list(CreateInterfaceTreesInner(sub, False)))
      for supers in itertools.product(*ifaces):
        yield TestInterface(supers, allow_default)
        all_supers = set(dump_supers(supers)) - set(supers)
        if all_supers:
          for i in range(1, min(len(all_supers) + 1, MAX_BASE_IFACE_PER_OBJECT)):
            for combo in itertools.combinations(all_supers, i):
              yield TestInterface(list(combo) + list(supers), allow_default)
      if allow_default:
        for i in range(len(split)):
          ifaces = []
          for cs in range(len(split)):
            sub = split[cs]
            if sub == 1:
              ifaces.append([TestInterface([], i == cs)])
            else:
              ifaces.append(list(CreateInterfaceTreesInner(sub, i == cs)))
          for supers in itertools.product(*ifaces):
            yield TestInterface(supers, False)
            all_supers = set(dump_supers(supers)) - set(supers)
            if all_supers:
              for i in range(1, min(len(all_supers) + 1, MAX_BASE_IFACE_PER_OBJECT)):
                for combo in itertools.combinations(all_supers, i):
                  yield TestInterface(list(combo) + list(supers), allow_default)

  for num in range(1, MAX_BASE_IFACE_PER_OBJECT):
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

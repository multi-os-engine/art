#!/usr/bin/env python3
"""
Generate Smali test data.
"""

import sys
from pathlib import Path
import itertools
import functools
import json

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

{test_funcs}

{main_func}

# }}
"""

TEST_FUNCTION_TEMPLATE = """
#   public static void {fname}(String s, {farg} v) {{
#     try {{
#       System.out.printf("%s-{invoke_type:<9} {farg:>9}.{callfunc}()='%s'\\n", s, v.{callfunc}());
#       return;
#     }} catch (Error e) {{
#       System.out.printf("%s-{invoke_type} on {farg}: {callfunc}() threw exception!\\n", s);
#       e.printStackTrace(System.out);
#     }}
#   }}
.method public static {fname}(Ljava/lang/String;L{farg};)V
    .locals 7
    :call_{fname}_try_start
      const/4 v0, 2
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0,v1,v0

      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v3, "%s-{invoke_type:<9} {farg:>9}.{callfunc}()='%s'\\n"

      invoke-{invoke_type} {{p1}}, L{farg};->{callfunc}()Ljava/lang/String;
      move-result-object v4
      const/4 v0, 1
      aput-object v4, v1, v0

      invoke-virtual {{v2,v3,v1}}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      return-void
    :call_{fname}_try_end
    .catch Ljava/lang/Error; {{:call_{fname}_try_start .. :call_{fname}_try_end}} :error_{fname}_start
    :error_{fname}_start
      move-exception v3
      const/4 v0, 1
      new-array v1,v0, [Ljava/lang/Object;
      const/4 v0, 0
      aput-object p0, v1, v0
      sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
      const-string v4, "%s-{invoke_type} on {farg}: {callfunc}() threw exception!\\n"
      invoke-virtual {{v2,v4,v1}}, Ljava/io/PrintStream;->printf(Ljava/lang/String;[Ljava/lang/Object;)Ljava/io/PrintStream;
      invoke-virtual {{v3,v2}}, Ljava/lang/Error;->printStackTrace(Ljava/io/PrintStream;)V
      return-void
.end method
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

PERMUTATION_NOTE_TEMPLATE = """
#     System.out.println("Beginning permutation {0}");
    const-string v0, "Beginning permutation {0}"
    invoke-virtual {{v1,v0}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
"""

TEST_GROUP_INVOKE_TEMPLATE = """
#     {test_name}();
    invoke-static {{}}, {test_name}()V
"""
INSTANCE_TEST_TEMPLATE = """
#   public static void {test_name}() {{
#     System.out.println("Testing for type {ty}");
#     String s = "{ty}";
#     {ty} v = new {ty}();
.method public static {test_name}()V
    .locals 3
    sget-object v2, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v0, "Testing for type {ty}"
    invoke-virtual {{v2,v0}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V

    const-string v0, "{ty}"
    new-instance v1, L{ty};
    invoke-direct {{v1}}, L{ty};-><init>()V

    {invokes}

    const-string v0, "End testing for type {ty}"
    invoke-virtual {{v2,v0}}, Ljava/io/PrintStream;->println(Ljava/lang/Object;)V
    return-void
.end method
#     System.out.println("End testing for type {ty}");
#   }}
"""

TEST_INVOKE_TEMPLATE = """
#     {fname}(s, v);
    invoke-static {{v0, v1}}, {fname}(Ljava/lang/String;L{farg};)V
"""

class MainFunc:
  def __init__(self):
    self.tests = set()
    self.global_funcs = set()

  def add_instance(self, it):
    self.tests.add(it)

  def add_func(self, f):
    self.global_funcs.add(f)

  def dump(self):
    all_tests = sorted(self.tests)
    test_invoke = ""
    test_groups = ""
    for t in all_tests:
      test_groups += t.dump()
    for p in itertools.permutations(sorted(all_tests)):
      test_invoke += PERMUTATION_NOTE_TEMPLATE.format(list(map(lambda a: a.get_name(), p)))
      for t in p:
        test_invoke += TEST_GROUP_INVOKE_TEMPLATE.format(test_name=t.get_name())
    main_func = MAIN_FUNCTION_TEMPLATE.format(test_group_invoke=test_invoke)

    funcs = ""
    for f in self.global_funcs:
      funcs += f.dump()
    return MAIN_CLASS_TEMPLATE.format(test_groups=test_groups,
                                      main_func=main_func, test_funcs=funcs)


@functools.total_ordering
class InstanceTest:
  def __init__(self, main, ty):
    self.ty = ty
    self.main = main
    self.funcs = set()
    self.main.add_instance(self)

  def __lt__(self, o):
    return self.get_name() < o.get_name()
  def __eq__(self, o):
    return self.get_name() == o.get_name()

  def dump(self):
    func_invokes = ""
    for f in sorted(self.funcs, key=lambda a: (a.func, a.farg)):
      func_invokes += TEST_INVOKE_TEMPLATE.format(fname=f.get_name(),
                                                  farg=f.farg)

    return INSTANCE_TEST_TEMPLATE.format(test_name=self.get_name(), ty=self.ty,
                                         invokes=func_invokes)
  def get_name(self):
    return "TEST_NAME_"+self.ty

  def __hash__(self):
    return hash(self.ty)

  def __eq__(self, o):
    return o.ty == self.ty

  def add_func(self, f):
    self.main.add_func(f)
    self.funcs.add(f)

@functools.total_ordering
class Func:
  def __init__(self, func, farg, invoke):
    self.func = func
    self.farg = farg
    self.invoke = invoke

  def __lt__(self, o):
    return self.get_name() < o.get_name()
  def __eq__(self, o):
    return self.get_name() == o.get_name()


  def dump(self):
    return TEST_FUNCTION_TEMPLATE.format(fname=self.get_name(), farg=self.farg, invoke_type=self.invoke, callfunc=self.func)

  def get_name(self):
    return "Test_Func_{}_{}_{}".format(self.func, self.farg, self.invoke)

  def __eq__(self, o):
    return o.farg == self.farg and o.invoke == self.invoke and o.func == self.func

  def __hash__(self):
    return hash((self.farg, self.invoke))

def FlattenClasses(classes, c):
  while c != None:
    yield c
    c = classes.get(c['super'])

def FlattenClassMethods(classes, c):
  for c1 in FlattenClasses(classes, c):
    for meth in c1['methods']:
      yield meth

def FlattenIFaces(dat, c):
  def GetIFaces(cl):
    for i2 in cl['implements']:
      yield dat['interfaces'][i2]
      yield from GetIFaces(dat['interfaces'][i2])

  for cl in FlattenClasses(dat['classes'], c):
    yield from GetIFaces(cl)

def FlattenIFaceMethods(dat, i):
  yield from i['methods']
  for i2 in FlattenIFaces(dat, i):
    yield from i2['methods']

def MakeMainClass(dat):
  m = MainFunc()
  for c in dat['classes'].values():
    i = InstanceTest(m, c['name'])
    for clazz in FlattenClasses(dat['classes'], c):
      for meth in FlattenClassMethods(dat['classes'], clazz):
        i.add_func(Func(meth, clazz['name'], 'virtual'))
      for iface in FlattenIFaces(dat, clazz):
        for meth in FlattenIFaceMethods(dat, iface):
          # i.add_func(Func(meth, iface['name'], 'virtual'))
          i.add_func(Func(meth, clazz['name'], 'virtual'))
          i.add_func(Func(meth, iface['name'], 'interface'))
  return m


def main(argv):
  smali_dir = Path(argv[1])
  if not smali_dir.exists() or not smali_dir.is_dir():
    print("{} is not a valid smali dir".format(smali_dir), file=sys.stderr)
    sys.exit(1)
  print(MakeMainClass(json.loads((smali_dir / "classes.json").open().read())).dump())

if __name__ == '__main__':
  main(sys.argv)

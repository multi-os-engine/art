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
Generate java test files for test 966.
"""

import functools
import generate_smali as base
import operator
import os
import subprocess
import sys
from contextlib import contextmanager
from pathlib import Path

BUILD_TOP = os.getenv("ANDROID_BUILD_TOP")
if BUILD_TOP is None:
  print("ANDROID_BUILD_TOP not set. Please run build/envsetup.sh", file=sys.stderr)
  sys.exit(1)

# Allow us to import mixins.
sys.path.append(str(Path(BUILD_TOP)/"art"/"test"/"utils"/"python"))

import testgen.mixins as mixins

class JavaConverter(mixins.DumpMixin, mixins.Named, mixins.JavaFileMixin):
  """
  A class that can convert a SmaliFile to a JavaFile.
  """
  def __init__(self, inner):
    self.inner = inner

  def get_name(self):
    return self.inner.get_name()

  def __str__(self):
    out = ""
    for line in str(self.inner).splitlines(keepends = True):
      if line.startswith("#"):
        out += line[1:]
    return out

@contextmanager
def flush(*out):
  """
  Simple context manager to force the given files to be flushed before and after the contained
  code. This is useful to make sure that any logs are fully written out before doing
  multithreaded/multiprocess operations.
  """
  for o in out:
    o.flush()
  yield
  for o in out:
    o.flush()

class Compiler:
  """
  A class that encapsulates compiling a set of java files.
  """
  def __init__(self, sources, javac, finish_compile, src_dir, scratch_dir):
    self.javac = javac
    self.finish_compile = finish_compile
    self.src_dir = src_dir
    self.scratch_dir = scratch_dir
    self.sources = sources

  def finish_compilation(self):
    """
    Run the program that will do any last setup.
    """
    cmd = [str(self.finish_compile), str(self.scratch_dir)]
    print("Running compile finish command: {} {}".format(*cmd))
    with flush(sys.stdout, sys.stderr):
      subprocess.check_call(cmd)
    print("Compilation finished")

  def compile_files(self, files):
    """
    Compile the files given with the arguments given.
    """
    files = list(map(str, files))
    cmd = [str(self.javac), str(self.scratch_dir)] + files
    print("Running compile command: {} {} <{} files>".format(str(self.javac),
                                                             str(self.scratch_dir),
                                                             len(files)))
    with flush(sys.stdout, sys.stderr):
      subprocess.check_call(cmd)
    print("Compiled {} files".format(len(files)))

  def execute(self):
    """
    Compiles this test, doing partial compilation as necessary.
    """
    # Compile Main and all classes first. Force all interfaces to be default so that there will be
    # no compiler problems (works since classes only implement 1 interface).
    files = []
    for f in self.sources:
      if isinstance(f, base.TestInterface):
        compilable_iface = f.get_specific_version(base.InterfaceType.default)
        out_file = JavaConverter(compilable_iface).dump(self.src_dir)
      else:
        out_file = JavaConverter(f).dump(self.src_dir)
      files.append(out_file)
    self.compile_files(files)

    # Now we compile the interfaces
    ifaces = set(i for i in self.sources if isinstance(i, base.TestInterface))
    while len(ifaces) != 0:
      # Find those ifaces where there are no (uncompiled) interfaces that are subtypes.
      tops = set(filter(lambda a: not any(map(lambda i: a in i.get_super_types(), ifaces)), ifaces))
      files = []
      # Dump these ones, they are getting compiled.
      for f in tops:
        out = JavaConverter(f)
        out_file = out.dump(self.src_dir)
        files.append(out_file)
      # Force all superinterfaces of these to be empty so there will be no conflicts
      overrides = functools.reduce(operator.or_, map(lambda i: i.get_super_types(), tops), set())
      for overridden in overrides:
        out = JavaConverter(overridden.get_specific_version(base.InterfaceType.empty))
        out_file = out.dump(self.src_dir)
        files.append(out_file)
      self.compile_files(files)
      # Remove these from the set of interfaces to be compiled.
      ifaces -= tops
    self.finish_compilation()
    return

def main(argv):
  """
  This program will generate and build all the java files needed to run this test.

  Usage: ./util-src/generate_java.sh build_cmd finish_cmd src_dir scratch_dir expected

  build_cmd:   This is a program that will be invoked in order to compile some set of java files.
               The set of files it is given will not have any incompatibilities between them that
               would prevent a conforming java language compiler from accepting them. This program
               must be given as a full path.

               The arguments passed to this program are:
               `scratch_dir java_file_1 java_file_2 ... java_file_n`
               with scratch_dir being the same directory as was passed to this program and
               java_file_1 to java_file_n being n source compatible java files to compile. The
               script may make arbitrary changes to scratch_dir but should refrain from making
               changes to any other parts of the filesystem. The sets of files that the program is
               invoked with will be the same as those needed to compile the code in the least
               amount of steps without source incompatibilities, assuming that the compiler
               translates each file individually and overwrites any preexisting copy in the
               scratch directory.

               The whole compilation will be considered to have failed if this script exits
               non-zero.

  finish_cmd:  This is a program that will perform any cleanup or final steps needed to create a
               runnable test file once all the build scripts has been run. If necessary it should
               process whatever artifacts the previous invocations of the build_cmd left in the
               scratch_dir and turn them into whatever files are needed to run the test. It should
               ensure that the version of every file used for the test is the last version of said
               file to be passed to build_cmd.

               The arguments passed to this program are: `scratch_dir`.

               The whole compilation will be considered to have failed if this script exits
               non-zero.

  src_dir:     This directory is where source files will be placed. After this script completes
               the java language source's in this directory will be the same as the last ones
               version passed to build_cmd.

  scratch_dir: This is a folder that is passed to build_cmd and finish_cmd as their first
               arguments. The contents of this folder will only be affected by build_cmd and
               finish_cmd and these scripts should use files within the folder to communicate
               between invocations and maintain state.

  expected:    This is the file we will write the expected output of running this test to.
  """
  javac_exec = Path(argv[1])
  if not javac_exec.exists() or not javac_exec.is_file():
    print("{} is not a shell script".format(javac_exec), file=sys.stderr)
    sys.exit(1)
  finish_compile_exec = Path(argv[2])
  if not finish_compile_exec.exists() or not finish_compile_exec.is_file():
    print("{} is not a shell script".format(javac_exec), file=sys.stderr)
    sys.exit(1)
  src_dir = Path(argv[3])
  if not src_dir.exists() or not src_dir.is_dir():
    print("{} is not a valid source dir".format(src_dir), file=sys.stderr)
    sys.exit(1)
  scratch_dir = Path(argv[4])
  if not scratch_dir.exists() or not scratch_dir.is_dir():
    print("{} is not a valid scratch dir".format(scratch_dir), file=sys.stderr)
    sys.exit(1)
  expected_txt = Path(argv[5])
  mainclass, all_files = base.create_all_test_files()

  with expected_txt.open('w') as out:
    print(mainclass.get_expected(), file=out)
  print("Wrote expected output")

  Compiler(all_files, javac_exec, finish_compile_exec, src_dir, scratch_dir).execute()

if __name__ == '__main__':
  main(sys.argv)

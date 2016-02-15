/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.InputStreamReader;
import java.util.Arrays;
import java.util.Comparator;

public class Main implements Comparator<Main> {
  // Whether to test local unwinding. Libunwind uses linker info to find executables. As we do
  // not dlopen at the moment, this doesn't work, so keep it off for now.
  public final static boolean TEST_LOCAL_UNWINDING = true;

  // Unwinding another process, modelling debuggerd. This doesn't use the linker, so should work
  // no matter whether we're using dlopen or not.
  public final static boolean TEST_REMOTE_UNWINDING = true;

  private boolean secondary;

  private boolean full_signatures;

  private boolean passed;

  public Main(boolean secondary, boolean full_signatures) {
      this.secondary = secondary;
      this.full_signatures = full_signatures;
  }

  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
      boolean secondary = false;
      boolean full_signatures = false;
      for (String arg : args) {
        if (arg.equals("--secondary")) {
          secondary = true;
        }
        if (arg.equals("--full-signatures")) {
          full_signatures = true;
        }
      }
      new Main(secondary, full_signatures).run();
  }

  private void run() {
      if (secondary) {
          if (!TEST_REMOTE_UNWINDING) {
              throw new RuntimeException("Should not be running secondary!");
          }
          runSecondary();
      } else {
          runPrimary();
      }
  }

  private void runSecondary() {
      foo();
      throw new RuntimeException("Didn't expect to get back...");
  }

  private void runPrimary() {
      // First do the in-process unwinding.
      if (TEST_LOCAL_UNWINDING && !foo()) {
          System.out.println("Unwinding self failed.");
      }

      if (!TEST_REMOTE_UNWINDING) {
          // Skip the remote step.
          return;
      }

      // Fork the secondary.
      String[] cmdline = getCmdLine();
      String[] secCmdLine = new String[cmdline.length + 1];
      System.arraycopy(cmdline, 0, secCmdLine, 0, cmdline.length);
      secCmdLine[secCmdLine.length - 1] = "--secondary";
      Process p = exec(secCmdLine);

      try {
          int pid = getPid(p);
          if (pid <= 0) {
              throw new RuntimeException("Couldn't parse process");
          }

          // Wait until the forked process had time to run until its sleep phase.
          try {
              InputStreamReader stdout = new InputStreamReader(p.getInputStream(), "UTF-8");
              BufferedReader lineReader = new BufferedReader(stdout);
              while (!lineReader.readLine().contains("Going to sleep")) {
              }
          } catch (Exception e) {
              throw new RuntimeException(e);
          }

          if (!unwindOtherProcess(full_signatures, pid)) {
              System.out.println("Unwinding other process failed.");
          }
      } finally {
          // Kill the forked process if it is not already dead.
          p.destroy();
      }
  }

  private static Process exec(String[] args) {
      try {
          return Runtime.getRuntime().exec(args);
      } catch (Exception exc) {
          throw new RuntimeException(exc);
      }
  }

  private static int getPid(Process p) {
      // Could do reflection for the private pid field, but String parsing is easier.
      String s = p.toString();
      if (s.startsWith("Process[pid=")) {
          return Integer.parseInt(s.substring("Process[pid=".length(), s.indexOf(",")));
      } else {
          return -1;
      }
  }

  // Read /proc/self/cmdline to find the invocation command line (so we can fork another runtime).
  private static String[] getCmdLine() {
      try {
          BufferedReader in = new BufferedReader(new FileReader("/proc/self/cmdline"));
          String s = in.readLine();
          in.close();
          return s.split("\0");
      } catch (Exception exc) {
          throw new RuntimeException(exc);
      }
  }

  public boolean foo() {
      // Call bar via Arrays.binarySearch.
      // This tests that we can unwind from framework code.
      Main[] array = { this, this, this };
      Arrays.binarySearch(array, 0, 3, this /* value */, this /* comparator */);
      return passed;
  }

  public int compare(Main lhs, Main rhs) {
      passed = bar(secondary);
      // Returning "equal" ensures that we terminate search
      // after first item and thus call bar() only once.
      return 0;
  }

  public boolean bar(boolean b) {
      if (b) {
          return sleep(2, b, 1.0);
      } else {
          return unwindInProcess(full_signatures, 1, b);
      }
  }

  // Native functions. Note: to avoid deduping, they must all have different signatures.

  public native boolean sleep(int i, boolean b, double dummy);

  public native boolean unwindInProcess(boolean full_signatures, int i, boolean b);
  public native boolean unwindOtherProcess(boolean full_signatures, int pid);
}

#!/usr/bin/env python
#
# Copyright (C) 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Separate out methods from an oatdump."""

import codecs
import os
import re
import string
import sys

_METHOD_START_RE = re.compile(r'^\s+[0-9]+:\s+[^\s]+\s+([^\s(]+)[^)]*\)\s+\(dex_method_idx=([0-9]+)\)$')
#                                   method number in class
#                                             return type
#                                                       method name (group 1)
#                                                               parameter list
#                                                                    closing parenthesis
#                                                                                           dex_idx (group 2)
                                                               
def FlushCodeBuf(code_buf, code_buf_last_suspend, out):
  # Flush the buffer to out if existing.
  if out is not None:
    if code_buf is not None:
      count = 0
      if code_buf_last_suspend == -1:
        # Just really large.
        code_buf_last_suspend = 10000000
      for line in code_buf:
        if count <= code_buf_last_suspend:
          out.write(line)
          out.write('\n')
        else:
          break
        count = count + 1

def ProcessFile(filename, dirname):

  current_output = None
  state = 0
  code_buf = None
  code_buf_last_suspend = -1

  with open(filename, 'r') as f:
    # Read line by line, as oat dumps are huge.
    for raw_line in f:
      # Are we starting a new class?
      if "type_idx=" in raw_line:
        if state != 0:
          FlushCodeBuf(code_buf, -1, current_output)
          code_buf = None
        state = 0
      if state == 0:
        # Is this the start of a method?
        m = _METHOD_START_RE.search(raw_line)
        if m:
          # Yes.
          current_output = open(dirname + "/" + m.group(1) + "." + m.group(2) + ".dump", 'w')
          current_output.write(raw_line)
          state = 1
      else:
        # Next method?
        m = _METHOD_START_RE.search(raw_line)
        if m:
          # Stop and close.
          FlushCodeBuf(code_buf, -1, current_output)
          code_buf = None
          current_output.close()
          current_output = open(dirname + "/" + m.group(1) + "." + m.group(2) + ".dump", 'w')
          current_output.write(raw_line)
          state = 1
          # Process next line.
          continue
        if state==1:
          # Expect "DEX CODE:".
          if raw_line.strip() != "DEX CODE:":
            # Error, back to state 0.
            state = 0
            continue
          current_output.write(raw_line)
          state = 2
        elif state==2:
          # Reading dex lines, terminate on OAT_DATA.
          current_output.write(raw_line)
          if raw_line.strip() == "OAT DATA:":
            state = 3
            continue
        elif state == 3:
          # Oat data.
          stripped = raw_line.strip()
          if stripped.startswith("CODE:"):
            state = 4
            code_buf_last_suspend = -1
            code_buf = []
            # TODO: Keep size
            current_output.write("CODE:")
            continue
          # Use if-elif for better readability.
          if stripped.startswith("frame_size"):
            current_output.write(raw_line)
          elif stripped.startswith("core_spill_mask"):
            current_output.write(raw_line)
          elif "/" in stripped:
            # Should be the vreg to physreg mapping
            current_output.write(raw_line)
          # Ignore the rest.
        elif state == 4:
          # Code line. strip the address.
          stripped = raw_line.strip()
          # Assume address is "0xXXXXXXXX: "
          if stripped.startswith("0x"):
            stripped = stripped[12:]
            # If the next thing is "00000000," assume we ran into the literal pool.
            if stripped.startswith("00000000") and code_buf_last_suspend > 0:
              FlushCodeBuf(code_buf, code_buf_last_suspend, current_output)
              state = 0
              continue
            if "(addr " in stripped:
              # Remove helping text.
              stripped = stripped[0:stripped.find("(addr")]
          else:
            # Not an address. Remember.
            code_buf_last_suspend = len(code_buf)
          code_buf.append(stripped)
  FlushCodeBuf(code_buf, -1, current_output)

def main():
  oat_dump_file = sys.argv[1]
  methods_dir = sys.argv[2]
  ProcessFile(oat_dump_file, methods_dir)
  sys.exit(0)


if __name__ == '__main__':
  main()

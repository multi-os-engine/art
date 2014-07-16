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

infile = open("opcodes.txt")

def indentation(indent):
    return ' ' * indent

class Instruction:
    def dump(self, comma, indent):
        pass

class UndefinedInstruction(Instruction):
    def dump(self, comma, indent):
        print indentation(indent) + "{kX86UndefinedOpcode, {0, 0, 0}}" + ("," if comma else "")

class X86Instruction(Instruction):
    def __init__(self, opcode, operands):
        self.opcode = opcode
        self.operands = operands

    def dump(self, comma, indent):
        trailingcomma = comma
        line = indentation(indent) + "{kX86Instruction, {"
        nops = 0
        comma = False
        for op in self.operands:
            line += (", " if comma else "") + op
            nops += 1
            comma = True
        pad = 3 - nops
        while pad > 0:
            line += (", " if comma else "") + "0"
            pad -= 1
            comma = True
        line += "}}" + ("," if trailingcomma else "")
        line += "  // " + self.opcode
        print line

class GroupInstruction(Instruction):
    def __init__(self, groupno):
        self.groupno = groupno

    def dump(self, comma, indent):
        print indentation(indent) + "{" + str(self.groupno) + \
            ", {0, 0, 0}}" + ("," if comma else "") + "  // group " + str(self.groupno)

class Table:
    def __init__(self, name):
        self.name = name
        self.instructions = []

    def add_instruction(self, inst):
        self.instructions.append(inst)

    def dump(self):
        print
        print "X86Instruction x86_" + self.name + "[] = {"
        comma = True
        i = len(self.instructions)
        for inst in self.instructions:
            inst.dump(comma, 2)
            i -= 1
            if i == 1:
                comma = False
        print "};"

def parse_operands(operands):
    f = operands.split(",")
    if len(f) == 0:
        return []

    result = []
    for op in f:
        operand = ""
        sep = ""
        for ch in op:
            if ch == 'r':
                operand = operand + sep + "kX86OpReg"
                break
            else:
                operand = operand + sep + "kX86Op_" + ch
            sep = "|"
        result.append(operand)
    return result


def parse_table(name):
    table = Table(name)

    while True:
        line = infile.readline()
        if len(line) == 0:		# premature eof
            break
        line = line.strip()
        if line == 'end':		# end of table
            break
        if len(line) == 0:		# blank line
           continue
        if line[0] == '#':		# commend
            continue

        fields = line.split()
        opcode = fields[0]

        if opcode == "undefined":                             # undefined opcode
            table.add_instruction(UndefinedInstruction())
        elif opcode == 'group':                          # group
            table.add_instruction(GroupInstruction(fields[1]))
        else:
            if len(fields) > 1:
                table.add_instruction(X86Instruction(opcode, parse_operands(fields[1])))
            else:
                table.add_instruction(X86Instruction(opcode, []))
    return table

class Group:
    def __init__(self, num, op):
        self.groupno = num
        self.opcode = op
        self.instructions = []

    def add_instruction(self, inst):
        self.instructions.append(inst)

    def dump(self, comma, indent):
        trailingcomma = comma
        print indentation(indent) + "{" + str(self.groupno) + ", 0x" + self.opcode + ", {"
        comma = True
        i = len(self.instructions)
        for inst in self.instructions:
            inst.dump(comma, indent + 2)
            i -= 1
            if i == 1:
                comma = False
        print indentation(indent) + "}}" + ("," if trailingcomma else "")

def parse_group(groupno, opcode):
    group = Group(groupno, opcode)

    i = 0
    while i < 8:
        line = infile.readline()
        if len(line) == 0:		# premature eof
            continue
        line = line.strip()
        if line == "end":
            break
        if len(line) == 0:		# blank
            continue
        if line[0] == '#':		# comment
            continue
        i += 1

        fields = line.split()
        opcode = fields[0]
        if opcode == 'undefined':
            group.add_instruction(UndefinedInstruction())
        else:
            if len(fields) > 1:
                group.add_instruction(X86Instruction(opcode, parse_operands(fields[1])))
            else:
                group.add_instruction(X86Instruction(opcode, []))

    return group

tables = []
groups = []

def dump_tables():
    for table in tables:
        table.dump()

def dump_groups():
    print
    print "X86Group x86_groups[] = {"
    for group in groups:
       group.dump(True, 2)
    print "  {0xff, 0, {"
    for i in range(8):
        print "    {kX86UndefinedOpcode, {0, 0, 0}}" + ("," if i < 7 else "")
    print "  }}";
    print "};"


# parse the file data
while True:
    line = infile.readline()
    if len(line) == 0:		# end of file
       break
    line = line.strip()
    if len(line) == 0:		# blank line
       continue
    if line[0] == '#':		# comment
       continue
    fields = line.split()
    if len(fields) > 0:
        if fields[0] == 'table':
            tables.append(parse_table(fields[1]))
        elif fields[0] == 'group':
            groups.append(parse_group(fields[1], fields[2]))

print '/*'
print ' * Copyright (C) 2013 The Android Open Source Project'
print ' *'
print ' * Licensed under the Apache License, Version 2.0 (the "License");'
print ' * you may not use this file except in compliance with the License.'
print ' * You may obtain a copy of the License at'
print ' *'
print ' *      http://www.apache.org/licenses/LICENSE-2.0'
print ' *'
print ' * Unless required by applicable law or agreed to in writing, software'
print ' * distributed under the License is distributed on an "AS IS" BASIS,'
print ' * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.'
print ' * See the License for the specific language governing permissions and'
print ' * limitations under the License.'
print ' */'
print
print "// This file is generated by buildx86opcodes.py.  It's probably best not"
print "// to modify it manually."
print

print '#include "x86_opcodes.h"'
print
print 'namespace art {'
print

dump_tables()
dump_groups()

print
print '}        // namespace art'


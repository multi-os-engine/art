#!/bin/bash
set -e
set -o xtrace

class_dir="$1"
shift
${JAVAC} -d $class_dir $@

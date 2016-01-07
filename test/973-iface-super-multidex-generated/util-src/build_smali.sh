#!/bin/bash
OUTPUT_FILE="$1"
shift
FILES="$@"
${SMALI} -JXmx512m ${SMALI_ARGS} --output $OUTPUT_FILE $FILES

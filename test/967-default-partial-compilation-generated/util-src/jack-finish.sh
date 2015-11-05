#!/bin/bash
set -e
set -o xtrace

jack_dir="$1"
jack_flags="$jack_dir/jack_flags"
jack_files="$jack_dir/jack_files"

# Get the imports in the right order. We added them to the end so we need to
# reverse them with 'tac'. We then write them to a file which we will pass to jack
# for the flags.
tac $jack_files | awk '{print "--import " $0}' > $jack_flags

${JACK} -D jack.import.type.policy=keep-first \
        -D jack.java.source.version=1.8 \
        @$jack_dir/jack_flags \
        --output-dex .

# Create that jar file needed to run the tests.
zip $TEST_NAME.jar classes.dex

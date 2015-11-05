#!/bin/bash
set -e
set -o xtrace

jack_dir="$1"
jack_files="$jack_dir/jack_files"
shift

new_jack_file=$(mktemp --tmpdir=${jack_dir} --suffix=-src.jack ${TEST_NAME}-INTERMEDIATE-XXXXXX)

# Build the new jack file.
${JACK} -D jack.java.source.version=1.8 --output-jack $new_jack_file "$@"

# Record the name of it.
echo $new_jack_file >> $jack_files

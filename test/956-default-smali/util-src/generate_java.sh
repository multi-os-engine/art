#!/bin/bash

if [ "$#" -ne "2" ]; then
  echo "Usage: ./regen_java.sh smali_dir java_dir"
  exit 1
fi

for f in `find "$1" -type f -name "*.smali" | xargs -i basename -s .smali \{\}`; do
  grep -E "^#" "$1/${f}.smali" | sed "s:#::" > "${2}/${f}.java"
done

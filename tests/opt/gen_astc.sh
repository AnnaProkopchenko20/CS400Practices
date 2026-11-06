#!/usr/bin/env bash
# Fills in only the MISSING NAME.astc files (those whose dump touches This/While
# nodes, which were not hand-derived). Existing .astc files are left alone.
# Read each generated file before committing: it must show the tree AFTER removal.
# usage (project root): tests/opt/gen_astc.sh path/to/compiler
set -euo pipefail
C="${1:?path to compiler}"
for t in tests/opt/*.txt; do
  n="${t%.txt}"
  [ -f "$n.astc" ] || { "$C" --remove-unused --ast-checked "$t" > "$n.astc"; echo "generated $n.astc"; }
done

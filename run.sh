#!/bin/bash
# Build the compiler, run every tests/*.txt and compare with its .expected file.
# Valid program  -> expected = what `lli` prints
# Invalid program -> expected = the compiler's stderr line
# Optional: tests/NAME.ast is compared with `compiler --ast`.

set -e
mkdir -p build
(cd build && cmake .. > /dev/null && cmake --build . --target compiler)
set +e

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

check() {  # label actual_file expected_file
    if [ ! -f "$3" ]; then
        echo "MISSING  $1  (no $3)"; fail=$((fail+1))
    elif diff -u "$3" "$2" > "$tmp/diff"; then
        echo "PASS     $1"; pass=$((pass+1))
    else
        echo "FAIL     $1"; cat "$tmp/diff"; fail=$((fail+1))
    fi
}

for t in tests/*.txt; do
    base="${t%.txt}"; name=$(basename "$base")
    rm -f "$tmp/out.ll"

    ./build/compiler "$t" "$tmp/out.ll" > /dev/null 2> "$tmp/actual"
    if [ $? -eq 0 ]; then
        lli "$tmp/out.ll" > "$tmp/actual" 2>&1
    elif [ -f "$tmp/out.ll" ]; then
        echo "FAIL     $name  (output file written despite error)"; fail=$((fail+1))
    fi
    check "$name" "$tmp/actual" "$base.expected"

    if [ -f "$base.ast" ]; then
        ./build/compiler --ast "$t" > "$tmp/ast" 2>&1
        check "$name [ast]" "$tmp/ast" "$base.ast"
    fi
done

echo "-----------------------------"
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]

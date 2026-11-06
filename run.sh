#!/bin/bash
# Build the compiler, run every tests/ok/*.txt and tests/err/*.txt and compare with its .expected file.
# Valid program  -> expected = what `lli` prints
# Invalid program -> expected = the compiler's stderr line
# Optional: tests/NAME.ast is compared with `compiler --ast` (the PARSE tree).
#
# OPT=1 ./run.sh   -> every test is compiled with --remove-unused; the same .expected files must match.
# tests/opt/NAME.txt  ->  NAME.astc  = `--remove-unused --ast-checked`,
#                         NAME.expected = output, identical with and without the flag.

set -e
mkdir -p build
(cd build && cmake .. > /dev/null && cmake --build . --target compiler)
set +e

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

FLAG=""
if [ -n "$OPT" ]; then FLAG="--remove-unused"; echo "(running every test with $FLAG)"; fi

check() {  # label actual_file expected_file
    if [ ! -f "$3" ]; then
        echo "MISSING  $1  (no $3)"; fail=$((fail+1))
    elif diff -u "$3" "$2" > "$tmp/diff"; then
        echo "PASS     $1"; pass=$((pass+1))
    else
        echo "FAIL     $1"; cat "$tmp/diff"; fail=$((fail+1))
    fi
}

for t in tests/ok/*.txt tests/err/*.txt; do
    base="${t%.txt}"; name=$(basename "$base")
    rm -f "$tmp/out.ll"

    ./build/compiler $FLAG "$t" "$tmp/out.ll" > /dev/null 2> "$tmp/actual"
    if [ $? -eq 0 ]; then
        if [ -f "$base.in" ]; then
            lli "$tmp/out.ll" < "$base.in" > "$tmp/actual" 2>&1
        else
            lli "$tmp/out.ll" > "$tmp/actual" 2>&1
        fi
    elif [ -f "$tmp/out.ll" ]; then
        echo "FAIL     $name  (output file written despite error)"; fail=$((fail+1))
    fi
    check "$name" "$tmp/actual" "$base.expected"

    if [ -f "$base.ast" ]; then
        ./build/compiler --ast "$t" > "$tmp/ast" 2>&1
        check "$name [ast]" "$tmp/ast" "$base.ast"
    fi

    if [ -f "$base.ircheck" ]; then
        if bash "$base.ircheck" "$tmp/out.ll" > "$tmp/iractual" 2>&1; then
            echo "PASS     $name [ircheck]"; pass=$((pass+1))
        else
            echo "FAIL     $name [ircheck]"; cat "$tmp/iractual"; fail=$((fail+1))
        fi
    fi
done

# --- Practice 9: tests/opt ---------------------------------------------------
run_prog() {  # flag base out_file
    rm -f "$tmp/opt.ll"
    ./build/compiler $1 "$2.txt" "$tmp/opt.ll" > /dev/null 2> "$3"
    if [ $? -eq 0 ]; then
        if [ -f "$2.in" ]; then lli "$tmp/opt.ll" < "$2.in" > "$3" 2>&1
        else lli "$tmp/opt.ll" > "$3" 2>&1; fi
    fi
}

for t in tests/opt/*.txt; do
    [ -e "$t" ] || continue
    base="${t%.txt}"; name="opt/$(basename "$base")"

    ./build/compiler --remove-unused --ast-checked "$t" > "$tmp/astc" 2>&1
    check "$name [astc]" "$tmp/astc" "$base.astc"

    run_prog "" "$base" "$tmp/plain"
    check "$name [without flag]" "$tmp/plain" "$base.expected"

    run_prog "--remove-unused" "$base" "$tmp/flagged"
    check "$name [with flag]" "$tmp/flagged" "$base.expected"
done

# --- Practice 10: native terminal sessions -----------------------------------
for t in tests/io/*.txt tests/ffi/*.txt; do
    [ -f "$t" ] || continue
    base="${t%.txt}"
    name="${base#tests/}"
    link_sources=()
    if [[ "$t" == tests/ffi/* ]]; then
        link_sources+=("$base.c")
    fi
    if ! ./build/compiler $FLAG "$t" "$tmp/io.ll" ||
       ! llc -filetype=obj -relocation-model=pic "$tmp/io.ll" -o "$tmp/io.o" ||
       ! clang -fPIE "$tmp/io.o" "${link_sources[@]}" -o "$tmp/io-program"; then
        echo "FAIL     $name (native build failed)"
        fail=$((fail+1))
        continue
    fi

    found_session=false
    for input in "$base".*.in; do
        [ -f "$input" ] || continue
        found_session=true
        session="${input%.in}"
        label="${session#tests/}"
        if "$tmp/io-program" < "$input" > "$tmp/actual"; then
            check "$label" "$tmp/actual" "$session.expected"
        else
            echo "FAIL     $label (binary exited unsuccessfully)"
            fail=$((fail+1))
        fi
    done
    if ! "$found_session"; then
        echo "FAIL     $name (no input sessions)"
        fail=$((fail+1))
    fi
done

echo "-----------------------------"
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]

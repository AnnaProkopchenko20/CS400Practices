# Compiler for the i32 language

Hand-written lexer, recursive-descent parser, AST and LLVM IR code generation (C++, LLVM 18).

## Build

```bash
mkdir -p build && cd build && cmake .. && cmake --build . --target compiler
```

## Run

```bash
./build/compiler input.txt output.ll   # compile to LLVM IR
lli output.ll                          # run the IR
./build/compiler --tokens input.txt    # print the token list
./build/compiler --ast input.txt       # print the syntax tree
```

On error the compiler prints `compilation error: line L:C: ...` to stderr,
exits non-zero and writes no output file.

## Tests

```bash
./run.sh
```

Each `tests/NN_name.txt` has a `NN_name.expected` next to it: the program's output
(valid tests) or the error line (invalid tests). Valid tests may also have
`NN_name.ast` with the expected `--ast` dump.

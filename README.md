# Compiler for the i32 language

Hand-written lexer, recursive-descent parser, AST and LLVM IR code generation (C++, LLVM 18).

## Language

Types `i32`, `i64`, `bool`; `mut` variables; `+ - *`, `== !=`, unary `!`.
`if` / `else` and `while` take a block: `{` and `}` stand alone on their own lines, a block
holds at least one line and may end with `exit`. Every block is a scope: inner declarations
may shadow outer ones (with any type) and are gone after the block. See `grammar.ebnf`.

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
opt -passes=mem2reg -S output.ll       # the same IR with allocas promoted to registers (phi)
```

On error the compiler prints `compilation error: line L:C: ...` to stderr,
exits non-zero and writes no output file.

## Tests

```bash
./run.sh
```

Tests live in two folders:

- `tests/ok/NAME.txt`: a valid program. `NAME.expected` is what `lli` prints.
  `NAME.ast` is optional: the expected `--ast` dump.
- `tests/err/NAME.txt`: an invalid program. `NAME.expected` is the compiler's error line.

Names are `feature_case` (`if-else_nested`, `scope_shadow-type`); old tests keep their
`pN_` prefix from the practice they came from.
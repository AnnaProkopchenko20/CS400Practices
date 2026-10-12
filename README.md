# Compiler for the Language with Structs and Functions (Practice 6)

Hand-written lexer, recursive-descent parser, AST, semantic type/mutability checker, and LLVM IR code generator (C++20, LLVM 18).

## Language Overview

The language supports scalar types (`i32`, `i64`, `bool`), user-defined `struct` types, top-level `fn` declarations, recursive calls, field member access, nested initialisations, and structured control flow.

### Program Structure
Programs have a strict top-level section ordering:
```
program ::= { struct } { fn } { statement } exit_stmt
```
All struct definitions must precede all function definitions, which must precede all top-level statements, concluding with a mandatory `exit` statement.

### User-Defined Structs
Structs are declared at the top level with named fields:
```
struct Point
{
    i32 mut x
    i32 mut y
}

struct Circle
{
    Point center
    i64 mut radius
}
```
- Fields are constant by default unless marked `mut`.
- Fields can be built-in types or previously declared structs.
- A struct must contain at least one field and cannot contain itself directly.

### Initialisation
- **Positional Initialisation**: One value per field in declaration order:
  ```
  Point p{10, 20}
  Circle c{p, 5}
  ```
- **Copy Initialisation**: A struct object can be initialised from an existing object or function call of the same type:
  ```
  Point q{p}
  ```
- **Nested Initialisation**: Nested `{}` initialisers construct compound structs inline:
  ```
  Circle c{{10, 20}, 5}
  ```

### Top-Level Functions
Functions are declared at the top level with typed parameters and a return type:
```
fn fib := (i32 n) -> i32
{
    if n == 0
    {
        exit 0
    }
    if n == 1
    {
        exit 1
    }
    exit fib(n - 1) + fib(n - 2)
}

exit fib(10)
```
- Functions can be recursive and call other functions declared anywhere in the file.
- Parameters are passed by value, allocated in entry-block stack slots, and immutable.
- The function body is a block ending in `exit`, which maps to `ret` in LLVM IR.
- In LLVM IR, function names are mangled with the prefix `fn.` (e.g. `@fn.fib`) to prevent name collisions with runtime symbols.

### Field Access & Mutability Chains
- **Field Access**: Access fields via dot notation: `p.x`, `c.center.x`.
- **Field Assignment**: Assign to field chains using `:=`:
  ```
  Circle mut c{{10, 20}, 5}
  c.radius := 10        # valid: c and radius are both mut
  c.center.x := 15      # error: center is const
  ```
  Every link in the access chain from the base variable up to the targeted field must be mutable.
- **Whole-Struct Assignment**: `p1 := p2` is permitted only if `p1` is mutable and all of its fields at every level of nesting are mutable.

### Control Flow & Expressions
- `if` / `else` and `while` statements take blocks enclosed in `{` and `}` on their own lines.
- Expressions: arithmetic (`+`, `-`, `*`), comparisons (`==`, `!=`), logical negation (`!`).
- Widening: `i32` values automatically widen to `i64` in arithmetic, assignments, and function arguments.

## Build

```bash
mkdir -p build && cd build && cmake .. && cmake --build . --target compiler
```

## Run

```bash
./build/compiler input.txt output.ll   # compile to LLVM IR
lli output.ll                          # run the IR via LLVM interpreter
./build/compiler --tokens input.txt    # print lexer tokens
./build/compiler --ast input.txt       # print the AST representation
opt -passes=mem2reg -S output.ll       # optimize IR with SSA registers (phi)
```

On error, the compiler outputs `compilation error: line L:C: ...` to `stderr`, exits with a non-zero code, and produces no output file.

## Tests

Run the full automated test suite:
```bash
./run.sh
```

Tests are split into two directories:
- `tests/ok/NAME.txt`: Valid programs verified against expected `lli` execution output (`NAME.expected`) and optional AST dumps (`NAME.ast`). Includes `p6_v01` through `p6_v07` covering nested chains, field writes, struct parameters, struct return values, nested calls, type widening, and nested initialisers.
- `tests/err/NAME.txt`: Invalid programs verifying that compilation fails with the exact diagnostic line in `NAME.expected`. Includes `p6_e01` through `p6_e06` testing unknown fields, incorrect initialiser counts, writes to const links, argument count/type mismatches, function exit type mismatches, and misplaced function definitions.

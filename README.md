# Compiler for the Language with Structs, Functions and Lambdas (Practice 9)

Hand-written lexer, recursive-descent parser, AST, semantic type/mutability checker, an
optimization pass that removes unused variables and functions, and an LLVM IR code
generator (C++20, LLVM 18).

## Language Overview

The language supports scalar types (`i32`, `i64`, `bool`), user-defined `struct` types with member functions, top-level `fn` declarations, lambdas with capture lists, recursive calls, field member access, nested initialisations, and structured control flow.

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
./build/compiler input.txt output.ll                  # compile to LLVM IR
lli output.ll                                         # run the IR via LLVM interpreter
./build/compiler --remove-unused input.txt output.ll  # compile the optimized tree
```

### Command-line flags

All flags may appear anywhere on the command line, before, between or after the paths.
Without `--remove-unused` the compiler behaves exactly as before.

| Flag | What it does |
|---|---|
| `--tokens` | Print the lexer tokens. |
| `--ast` | Print the **parse** tree (before the checker) and stop. |
| `--ast-checked` | Print the tree after the checker, and after the pass if `--remove-unused` is given, and stop. |
| `--remove-unused` | Run the optimization pass after the checker and before code generation (see Optimizations). |
| `--keep-calls` | With `--remove-unused`: use the Task 1 rule, so any call or `read()` keeps its value. |
| `--opt-stats` | With `--remove-unused`: print rounds, removed counts and live-node counts to `stderr`. |

```bash
./build/compiler --remove-unused --ast-checked input.txt   # tree after the pass
./build/compiler --remove-unused --opt-stats input.txt out.ll
```

On error, the compiler outputs `compilation error: line L:C: ...` to `stderr`, exits with a non-zero code, and produces no output file. The pass never runs on a program the checker rejected.

## Tests

Run the full automated test suite:
```bash
./run.sh              # every test as before
OPT=1 ./run.sh        # tests/ok and tests/err again, each compiled with --remove-unused
```

Tests are split into directories:
- `tests/ok/NAME.txt`: Valid programs verified against expected `lli` execution output (`NAME.expected`) and optional AST dumps (`NAME.ast`).
- `tests/err/NAME.txt`: Invalid programs verifying that compilation fails with the exact diagnostic line in `NAME.expected`.
- `tests/opt/NAME.txt`: Programs for the optimization pass. `NAME.astc` is the tree printed by `--remove-unused --ast-checked`; `NAME.expected` is the program output, which must be identical with and without the flag. `tests/opt/gen_astc.sh <compiler>` creates any missing `.astc`; read the result before committing.

`tests/opt` covers: both warm-up programs, the three-function chain, a declaration inside an `if` block, a declaration kept because its call prints, a program where nothing is removed, a lambda capture that keeps its variable alive, a member function calling a sibling member function, and for the effects rule the `square`/`quiet`/`loud` program, an effect through a chain of two calls (`relay`), a mutating `bump` next to a pure `peek`, a function with `while` next to a recursive one, and a printing lambda next to a pure one.

## Optimizations

`--remove-unused` runs one more visitor pass after the semantic checker and before code
generation. It never runs on an invalid program and never calls `opt`. Code generation
is unchanged.

### What is removed

- **Unused variables.** A visitor counts, per declaration (not per name), the reads that
  resolve to it through `VarNameNode::decl`, which the checker already fills in. Two
  variables both called `x` are two keys. A read is a use in an initialiser, on the right
  of `:=`, in a condition, an argument, `print`, `exit`, the root of a field chain
  `p.x`, the object of a member call, a capture list, or a callable object call. The
  target of `y := 5` or `p.x := 5` is not a read. Declarations with zero reads are
  removed together with every assignment to them. Parameters, fields and lambda closure
  declarations are never removed.
- **Unused functions.** A second visitor counts the calls of every global function,
  wherever the call stands: in statements, other functions, member functions and lambda
  bodies. A function with zero calls is removed with its whole body. Member functions and
  lambdas stay.
- **Loop.** Each round does, in this order: recompute the effect analysis, remove
  variables, remove functions. The loop runs until a round removes nothing. Both
  removals share one loop, because removing a variable can take away the only call of a
  function, and removing a function can take away the only read of a variable.
- **Counters are fresh every round.** They are keyed by pointers, and a freed address may
  be reused by a later node.
- **Empty blocks stay.** An `if` or `while` whose block becomes empty stays, because its
  condition is still evaluated. Code generation accepts an empty block.

### Calls without effects (additional task)

A value is kept only if it contains `read()` or a call of a function with an effect.
A function has an effect if:
1. its body contains `print`, `read()` or any `while` (termination cannot be proved);
2. it is a mutating member function;
3. it can reach itself in the call graph, directly or through other functions (a call that
   never returns is observable);
4. it calls a function with an effect.

Rules 1 and 2 seed the set, rule 3 is a depth-first search from each function, and rule 4
is a fixpoint that adds callers until nothing changes. The call graph has every global
function, member function and lambda `call` as a node. A callable object call is a call
of its `call`, so a lambda that prints is not mistaken for a pure one. A call with no
resolved callee is treated as an effect. The set is recomputed every round, because
removals change the graph.

A mutating call stays even when its result is unused: in `i32 t{c.bump()}` the
declaration `t` is kept, while `i32 u{c.peek()}` goes. `mutates` is computed by the
checker, but it is not transitive for lambdas, so the optimizer builds its own graph
instead of trusting it for propagation.

`--keep-calls` switches back to the Task 1 rule: any call or `read()` keeps its value.

### Limits

- A variable that only feeds itself (`k := k + 1`, nobody else reads `k`) survives: the
  right side reads it. This is a limit of counting, not a bug.
- Two functions that only call each other, or one that calls itself, never reach zero
  calls and stay. Counting cannot see a cycle, for the same reason reference counting
  cannot free one.
- A lambda declared inside a removed function leaves its generated struct and `call`
  body behind, because lambda bodies live in the struct list. What that body calls stays
  too. This is safe but conservative.
- A function that only runs under a `while` is treated as having an effect, and is kept.

### Who frees a removed subtree

The compiler is written in C++. AST nodes are owned by `std::unique_ptr`, and no node has
a pointer back to its parent, so there are no ownership cycles and no reference counts.
When the pass rebuilds a statement list without a dead declaration, the old `unique_ptr`
is destroyed and the node and everything below it are freed immediately; nothing waits
for a collector. If nodes ever got a parent pointer it would have to be a raw pointer or
`weak_ptr`: with `shared_ptr` in both directions a parent and child would keep each other
alive and leak.

Non-owning raw pointers remain: `VarNameNode::decl`, `FunctionCallNode::fn_decl`,
`MemberCallNode::fn`, and the checker's own maps (`functions`, `scopes`,
`StructInfo::member_fns`). After a removal the ones that pointed at a removed node
dangle. Nothing dereferences them afterwards: the pass only removes a declaration with
zero reads (so no live node refers to it), and a function with zero calls, and
`main` only runs code generation, which reads `program->functions` and
`program->structs`, never the checker's maps.

**Measurement.** `Node::live` is a static counter, incremented in the `Node`
constructor and decremented in its destructor. `--opt-stats` prints it before and after
the pass. For `tests/opt/opt_warmup1_chain.txt` (`i32 x{10}`, `i32 y{x}`, `i32 z{15}`):

```
$ ./build/compiler --remove-unused --opt-stats tests/opt/opt_warmup1_chain.txt out.ll
[opt] rounds=3 removed_stmts=2 removed_functions=0 live_nodes 12 -> 6
```

Round 1 removes `y` (a `DeclNode`, its `InitNode` and a `VarNameNode`: 3 nodes), round 2
removes `x` (a `DeclNode`, an `InitNode` and a `NumberNode`: 3 nodes), round 3 removes
nothing and stops ($B = A - 6$, decreasing from 12 to 6 live nodes). A run with
`-fsanitize=address` on the same program is clean, with no leak and no use-after-free.

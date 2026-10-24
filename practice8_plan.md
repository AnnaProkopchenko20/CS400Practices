# Practice 8 implementation plan: callable objects and lambdas

Files I have not seen (semantic_checker.h, codegen.h, parser.h, the node headers for MemberCallNode, DeclNode, StructInfo, lexer states, main) are marked "unseen". Names there are guesses to adapt.

## 0. Decisions already taken, and decisions still open

Taken:
- Lexer and parser need no change for Task 1. Task 1 is a semantic rewrite: `name(args)` becomes `name.call(args)`.
- Lambda is a statement (`LambdaNode : StmtNode`), not an expression. It declares the closure variable itself.
- The parser only builds a `LambdaNode` with `LambdaCapture`s. The struct, `call` and the declaration are built in the semantic pass (capture types are unknown at parse time; `--ast` stays the parse tree).
- Captures are fields of the generated struct. `mut` capture = `mut` field. No reference capture.
- Order inside `visit_lambda`: resolve captures, create struct, create `call` node, check body in switched state, restore, register `call`, declare closure variable.

Former open points, now decided:
- A: `LambdaNode` stays in the tree and forwards to its lowered declaration.
- B: lambda values are rejected at each site (print, exit, argument, init, assign), not in `visit_var`.
- C: the explicit `add_k.call(5)` is allowed on a lambda.
- D: generated structs are collected in a separate vector and appended to `n.structs` after Phase 7.

## 1. Overview of changes by file

| File | Change |
|---|---|
| `nodes/lambda_capture.h` (new) | `LambdaCapture`: line, col, name, is_mut |
| `nodes/lambda_node.h` (new) | `LambdaNode : StmtNode` with captures, return type, params, body; after lowering: `lowered_struct` (raw), `lowered_decl` (owned) |
| `ast_fwd.h`, `ast.h` | forward declare and include the new nodes |
| `visitor.h` | add `visit_lambda(LambdaNode&)` |
| MemberCallNode (unseen) | add `is_callable_sugar`, `callable_name`; dump prints `CallableCall <name>` when set |
| DeclNode (unseen) | add `is_closure_init = false` (skips the lambda-value rejection for generated declarations) |
| StructInfo (shared, unseen) | add `is_lambda = false` |
| `parser.cpp/.h` | `at_lambda`, `parse_lambda`, `parse_captures`; changes in `parse_statement`, `parse_program`, `parse_member_fn` |
| `semantic_checker.cpp/.h` | state guard, call resolution, helpers, `visit_lambda`, rejection helpers, message changes, mutation flag fix |
| `codegen.cpp` | `visit_lambda` forwarder; `member_fn_ir_name` special case for lambda structs |
| main (unseen) | `--ast-checked` flag: dump after the semantic pass |

## 2. New nodes

```
struct LambdaCapture : Node {
    string name; bool is_mut;
    LambdaCapture(line, col, name, is_mut)
    dump(d): indent; print "Capture " + (is_mut ? "mut " : "") + name
}

struct LambdaNode : StmtNode {
    string name;
    vector<unique_ptr<LambdaCapture>> captures;
    string return_type;
    vector<unique_ptr<FuncParamNode>> params;   // moved out during lowering
    unique_ptr<BlockNode> body;                 // moved out during lowering

    // filled by the checker (Option A)
    StructDeclNode*         lowered_struct = nullptr;  // owned later by ProgramNode.structs
    unique_ptr<DeclNode>    lowered_decl;

    dump(d):
        indent; print "Lambda " + name + " -> " + return_type
        if lowered_struct == nullptr:        // parse tree (--ast)
            dump captures, params, body at d+1
        else:                                // after checking (--ast-checked)
            lowered_struct->dump(d+1)
            lowered_decl->dump(d+1)

    accept(v) = v.visit_lambda(*this)
}
```

Note: after `n.structs` receives the generated struct, the Program dump lists it too. Decide whether to hide generated structs there.

## 3. Parser

```
at_lambda():        // current token is 'fn'
    return at(keyword, FN)
       and toks[pos+1].kind == identifier
       and toks[pos+2] is operator ":="
       and toks[pos+3] is the '[' token        // confirm the lexer produces it (kind unknown)

parse_captures():                              // after '[' has been eaten
    list = []
    if at(']'): return list
    loop:
        is_mut = at(keyword, MUT); if is_mut: eat()
        tok = expect(identifier, "a captured variable name")
        list.push(LambdaCapture(tok.line, tok.col, tok.text, is_mut))
        if at(comma): eat() else break
    return list

parse_lambda():
    kw = eat()                                  // 'fn'
    name = expect(identifier)
    expect(":="); expect('[')
    captures = parse_captures()
    expect(']'); expect('(')
    params = parse_params()
    expect(')'); expect('->')
    ret = expect_type()
    end_of_line()
    body = parse_block(FN)
    if not body.exit_stmt: error_at(kw, "lambda '" + name + "' must end with 'exit'")
    return LambdaNode(kw.line, kw.col, name, captures, ret.text, params, body)

parse_statement():                              // add one branch
    if at(keyword, FN):
        if at_lambda(): return parse_lambda()   // no end_of_line: the block ends the statement
        error_at(*peek(), "function declarations are not allowed inside a block")
    ... existing branches ...

parse_program():                                // inside the loop, replace the FN branch
    else if at(keyword, FN):
        if at_lambda():
            section = Statements
            stmts.push(parse_lambda())
        else:
            existing fn logic (error if section > Functions)

parse_member_fn():                              // after expect(":=")
    if at('['): error_at(*peek(), "a member function cannot have a capture list")
```

Positions matter: duplicate capture errors point at the identifier of the second `k`.

## 4. Semantic checker

### 4.1 New members

```
struct StateGuard {                     // RAII: saves in constructor, restores in destructor
    scopes, current_fn, current_member_fn, current_struct_info, current_struct_node
}

struct LambdaHint { string name; vector<Scope> outer_scopes; StructInfo* outer_struct; }
vector<LambdaHint> lambda_hints;        // innermost lambda last

vector<unique_ptr<StructDeclNode>> generated_structs;   // Option D

struct DeferredCall: add  string display_name; bool is_sugar;
```

### 4.2 Fix existing state handling

```
visit_fn_decl / visit_member_fn_decl:
    prev = current_fn / current_member_fn      // instead of setting nullptr at the end,
    ...                                        // restore the previous values
```

### 4.3 Name resolution for calls

```
struct CallTarget { bool is_var, is_member_fn, is_global_fn; LookupResult var; MemberFnDeclNode* member; FunctionDeclNode* global; }

resolve_call_name(name):
    t.var = lookup_name(name)                       // local, param, field-of-this or capture
    t.is_var = (t.var.kind != NotFound)
    if current_struct_info: member_fns.find(name) -> t.member
    functions.find(name) -> t.global
    t.is_member_fn = t.member != null; t.is_global_fn = t.global != null
    return t
```

`check_expr(slot)` becomes:

```
check_expr(slot):
    maybe_rewrite_field(slot)
    maybe_rewrite_callable_call(slot)
    slot.accept(*this)

maybe_rewrite_callable_call(slot):
    fc = dynamic_cast<FunctionCallNode*>(slot); if not fc: return
    t = resolve_call_name(fc.name)
    if t.is_var and (t.is_member_fn or t.is_global_fn):
        fail(fc, "'" + name + "' is both a variable and a function: rename one of them")
    if not t.is_var: return                          // ordinary function call, handled by visit_fn_call
    obj = VarNameNode(fc.line, fc.col, fc.name)      // a field is rewritten to this.field when it is checked
    mc = MemberCallNode(fc.line, fc.col, obj, "call", move(fc.args))
    mc.is_callable_sugar = true; mc.callable_name = fc.name
    slot = mc
```

### 4.4 Argument and error helpers

```
check_call_args(at, args, params, display):
    if args.size != params.size:
        fail(at, "'" + display + "' takes N argument(s), got M")
    for i: check_expr(args[i]); reject_lambda_value(args[i], "passed to '" + display + "'")
           have/want rule as today (i32 -> i64 allowed)
           on mismatch: fail(args[i], "cannot pass argument i of '" + display + "' of type ... with a value of type ...")

mutation_error(at, mfn, display, is_sugar, through_const_field):
    if is_sugar: fail(at, "cannot call '" + display + "': its 'call' changes it, and it is not mut")
    else: existing two messages
```

`display` is the callable's name for sugar calls, and the member function name otherwise.

### 4.5 `visit_fn_call` (only real function calls reach it now)

```
visit_fn_call(n):
    t = resolve_call_name(n.name)
    if t.is_var: internal error                       // should have been rewritten
    if t.is_member_fn: return visit_member_fn_of_current_struct(n, t.member)
    if t.is_global_fn: return visit_global_fn_call(n, t.global)
    fail(n, "function '" + n.name + "' is not declared")

visit_member_fn_of_current_struct(n, m):
    n.fn_decl = m
    check_call_args(n, n.args, m.params, n.name)
    n.type = m.return_type
    if in_member_body_pass:
        call_edges[current_member_fn].insert(m); deferred_calls.push({&n, m, true, false, n.name, false})
    else if m.mutates and current_member_fn: directly_mutates.insert(current_member_fn)
    return nullptr

visit_global_fn_call(n, f):
    n.fn_decl = f
    check_call_args(n, n.args, f.params, n.name)
    n.type = f.return_type
```

The last `else` branch is new: outside Phase 4 the callee's `mutates` is already final, so a mutating call on `this` marks the caller immediately. This is what makes lambdas in Phase 7 work.

### 4.6 `visit_member_call`

```
visit_member_call(n):
    check_expr(n.object)
    type = n.object.type
    display = n.is_callable_sugar ? n.callable_name : n.fn_name
    sinfo = structs.find(type)
    if sinfo == end:
        if n.is_callable_sugar: fail(n, "'" + display + "' is not callable: it is " + type)   // built-in type
        fail(n, "type '" + type + "' is not a struct")

    if not n.is_callable_sugar and not sinfo.member_fns.count(n.fn_name)
       and sinfo.field_index.count(n.fn_name):           // obj.name(args) where name is a field
        idx = sinfo.field_index[n.fn_name]
        fa = FieldAccessNode(n.line, n.col, move(n.object), n.fn_name)
        fa.field_index = idx; fa.type = sinfo.fields[idx].type     // no second visit of the inner object
        n.object = fa
        n.is_callable_sugar = true; n.callable_name = n.fn_name; n.fn_name = "call"
        display = n.callable_name
        type = fa.type
        sinfo = structs.find(type)                            // may be end: built-in field type
        if sinfo == end: fail(n, "'" + display + "' is not callable: it is " + type)

    mit = sinfo.member_fns.find(n.fn_name)
    if mit == end:
        if n.is_callable_sugar: fail(n, "'" + display + "' is not callable: " + type + " has no member function 'call'")
        fail(n, "struct '" + type + "' has no member function '" + n.fn_name + "'")

    mfn = mit.second; n.fn = mfn
    check_call_args(n, n.args, mfn.params, display)
    n.type = mfn.return_type

    chain_mut = is_chain_mut(n.object); const_link = has_const_field_link(n.object)
    if in_member_body_pass:
        if is_rooted_at_this(n.object): call_edges[current_member_fn].insert(mfn)
        deferred_calls.push({&n, mfn, chain_mut, const_link, display, n.is_callable_sugar})
    else:
        if mfn.mutates:
            if current_member_fn and is_rooted_at_this(n.object): directly_mutates.insert(current_member_fn)
            if const_link: mutation_error(n, mfn, display, n.is_callable_sugar, true)
            if not chain_mut: mutation_error(n, mfn, display, n.is_callable_sugar, false)
    return nullptr
```

Phase 6 uses the stored `display_name` and `is_sugar` in the same `mutation_error`.

### 4.7 `visit_lambda`

```
visit_lambda(L):
    // 0. name
    check_new_name(L, L.name)
    if scopes.back().count(L.name): fail(L, "variable '" + L.name + "' is already declared in this block")

    // 1. captures, resolved in the enclosing state (nothing switched yet)
    seen = {}; fields = []; init_items = []; any_mut = false
    for cap in L.captures:
        if cap.name in seen: fail(cap, "'" + cap.name + "' is captured twice")
        seen.add(cap.name)
        r = lookup_name(cap.name)
        if r.kind == NotFound: fail(cap, "cannot capture '" + cap.name + "': no variable '" + cap.name + "' is visible here")
        fields.push({r.type, cap.name, cap.is_mut}); any_mut |= cap.is_mut
        init_items.push(VarNameNode(cap.line, cap.col, cap.name))

    // signature checks (same rules as Phase 2/3: share one helper)
    check_signature(params, return_type)      // reserved names, unknown types, duplicate params
    for p in L.params: if p.name in seen: fail(p, "parameter '" + p.name + "' has the same name as a capture")

    // 2. struct
    sname = unique_lambda_name(L.name)        // "lambda." + name; add a counter if already in `structs`
    info = StructInfo{ name: sname, is_lambda: true, fields, field_index }
    structs[sname] = info
    field_nodes = FieldDeclNode(type, name, is_mut) per capture
    call_node = MemberFnDeclNode(L.line, L.col, "call", L.return_type, move(L.params), move(L.body))
    struct_node = StructDeclNode(L.line, L.col, sname, field_nodes, [call_node]); call_node.struct_decl = struct_node

    // 3. body, in the lambda's own state
    {
        StateGuard guard(*this)
        lambda_hints.push({L.name, guard.saved_scopes, guard.saved_struct_info})
        current_struct_info = &structs[sname]; current_struct_node = struct_node
        current_fn = call_node; current_member_fn = call_node
        scopes = [ {} ]                       // fresh: captures arrive through fields of 'this'
        visit_member_fn_decl(call_node)
        lambda_hints.pop()
    }                                          // 5. state restored here

    // 6. register call and compute mutates
    structs[sname].member_fns["call"] = call_node
    if not in_member_body_pass:
        call_node.mutates = directly_mutates.contains(call_node)
    // inside Phase 4 the normal fixed point handles it (call_edges / directly_mutates)

    // 7. the closure variable, in the enclosing state (visit_decl checks the capture initialisers)
    decl = DeclNode(L.line, L.col, sname, L.name, any_mut, InitNode(L.line, L.col, init_items))
    decl.is_closure_init = true
    L.lowered_decl = decl; L.lowered_struct = struct_node
    decl.accept(*this)
    generated_structs.push(struct_node)
    return nullptr
```

Because `call` is registered only at step 6, a plain `call(...)` inside the body does not find it and fails with "function 'call' is not declared". If that message is not good enough, special-case it.

### 4.8 Scope isolation and "not captured"

```
fail_unknown_name(n, name):                  // used by visit_var when lookup_name returns NotFound
    if lambda_hints not empty:
        h = lambda_hints.back()
        if name visible in h.outer_scopes, or a field of h.outer_struct:
            fail(n, "'" + name + "' is not captured by lambda '" + h.name + "': add it to the capture list, [" + name + "]")
    fail(n, "variable '" + name + "' is used before its declaration")
```

### 4.9 Lambda-specific rules

```
reject_lambda_value(expr, role):
    if expr is a lambda-typed value and expr is VarNameNode v:
        fail(expr, "'" + v.name + "' is a lambda and cannot be " + role + ": its type has no name, so no parameter can have it")
```

Call it from: `check_call_args` ("passed to 'f'"), `visit_exit` and the top-level exit ("returned"), `visit_print` ("printed"), `visit_asmt` rhs and lhs ("assigned"), and the user-written paths of `visit_decl` and `check_init_item` ("used to initialise"). It must be skipped when `n.is_closure_init` is set, because a lambda may legitimately capture another lambda.

```
visit_field_access(n):                       // add at the start, after check_expr(n.object)
    if structs[object.type].is_lambda and n.object is not a ThisNode:
        fail(n, "'" + field + "' is a capture of a lambda and is not visible outside its body")

validate_lvalue(...):                        // in the bare this-field branch
    if structs[leaf.object.type].is_lambda:
        fail(at, "cannot assign to '" + field + "': the capture '" + field + "' is const, write [mut " + field + "]")
```

### 4.10 `visit_program` additions

- Phase 7 loops stay as they are.
- At the very end: `for g in generated_structs: n.structs.push_back(move(g))`, so codegen sees them.

## 5. Code generation

```
visit_lambda(L):
    return L.lowered_decl->accept(*this)     // the struct and `call` are already in n.structs

member_fn_ir_name(struct_name, fn_name):
    if struct_name starts with "lambda.": return struct_name + "_" + fn_name
    ... existing escaping ...
```

The special case matters: a lambda named `add_k` has an underscore, so the existing escaping would produce `lambda.add__k_call`, while the sheet shows `lambda.add_k_call`. The `.` in the struct name already prevents clashes with user names.

Everything else already works: `call` member calls, `this`, captures as fields, `if`/`while` inside a lambda body (`member_fn_ir_name` is used consistently). An empty struct (zero captures) should be fine in LLVM.

## 6. Order of implementation

1. Re-run the Practice 7 tests with the fixed `codegen.cpp`.
2. Task 1 only: `resolve_call_name`, `maybe_rewrite_callable_call`, helpers, rewritten `visit_fn_call` and `visit_member_call`, node flags and dump. Run the Task 1 sample program and its error cases.
3. State-guard fixes in `visit_fn_decl` and `visit_member_fn_decl`.
4. Nodes, visitor, parser. Check `--ast` on the Task 2 sample.
5. `visit_lambda` for captures only, one capture, no mut. Then params, then nested lambda, then lambda in a loop.
6. `fail_unknown_name`, scope isolation, capture errors, `reject_lambda_value`, field access and assignment messages.
7. Codegen forwarder, `member_fn_ir_name`, `--ast-checked`. Run the sample through `lli`.
8. Additional task: `[mut n]`, closure `mut` flag, mutation flags in Phase 7, the `[next]` vs `[mut next]` error.
9. Tests, `ai_usage.txt`, pull request to `practice-8`.

## 7. Messages the sheet fixes

| Case | Message (from the sheet) |
|---|---|
| name is both variable and function | `'twice' is both a variable and a function: rename one of them` |
| struct without `call` | `'p' is not callable: Point has no member function 'call'` |
| built-in variable called | `'n' is not callable: it is i32` |
| mutating call on const | `cannot call 'c': its 'call' changes it, and it is not mut` |
| missing capture | `'k' is not captured by lambda 'add_k': add it to the capture list, [k]` |
| assign to capture | `cannot assign to 'k': the capture 'k' is const, write [mut k]` |
| duplicate capture | `'k' is captured twice` |
| unknown capture | `cannot capture 'k': no variable 'k' is visible here` |
| lambda passed | `'times' is a lambda and cannot be passed to 'sq': its type has no name, so no parameter can have it` |

## 8. Tests to write

ok: struct with `call` both ways; callable field; callable called inside a member function; lambda without captures calling a global function; capture changed after declaration; lambda in a function capturing a parameter; lambda in a loop; lambda capturing another lambda; `print` and `read()` inside a lambda; (additional) counter, `mut` capture in a loop, two lambdas with own copies, const and mut capture in one list.

err: struct without `call`; call on `i32`; both variable and function; missing capture; assignment to a capture; duplicate capture; unknown capture; lambda passed as argument; lambda as `exit` value; `Counter c{0}; c()` (const mutating); `[next]` instead of `[mut next]`; lambda calling itself; parameter named like a capture.

## 9. Pitfalls checklist

- State restore: previous values restored, never set to `nullptr`.
- A generated closure declaration must skip `reject_lambda_value` (capturing a lambda is legal).
- Generated struct names must be unique; use `member_fn_ir_name` special case.
- Do not append to `n.structs` while Phase 4 iterates it.
- Check that `[` and `]` are tokens.
- Do not visit a `LambdaNode` twice (the lowering is not idempotent).
- Phase 7 lambdas need their `mutates` flag right after the body check; Phase 4 lambdas rely on the fixed point.

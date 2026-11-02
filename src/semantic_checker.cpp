#include "semantic_checker.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace
{
    bool is_int(const std::string& t) { return t == "i32" || t == "i64"; }
}

SemanticChecker::SemanticChecker() { scopes.emplace_back(); }

void SemanticChecker::fail(int line, int col, const std::string& msg)
{
    throw std::runtime_error(
        "line " + std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
}

void SemanticChecker::fail(const Node& at, const std::string& msg)
{
    fail(at.line, at.col, msg);
}

bool SemanticChecker::is_builtin_type(const std::string& t)
{
    return t == "i32" || t == "i64" || t == "bool";
}

bool SemanticChecker::is_known_type(const std::string& t)
{
    return is_builtin_type(t) || structs.count(t);
}

void SemanticChecker::check_new_name(const Node& at, const std::string& name)
{
    if (name == kw::READ || name == kw::PRINT)
        fail(at, "'" + name + "' is a reserved name");
    if (is_builtin_type(name))
        fail(at, "'" + name + "' is already declared");
}

SemanticChecker::LookupResult SemanticChecker::lookup_name(const std::string& name)
{
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
    {
        auto f = it->find(name);
        if (f != it->end())
            return {LookupResult::LocalVar, f->second, -1, f->second->type_name, f->second->is_mut};
    }
    if (current_struct_info)
    {
        auto fit = current_struct_info->field_index.find(name);
        if (fit != current_struct_info->field_index.end())
        {
            const auto& fi = current_struct_info->fields[fit->second];
            return {LookupResult::FieldOfThis, nullptr, fit->second, fi.type, fi.is_mut};
        }
    }
    return {LookupResult::NotFound, nullptr, -1, "", false};
}

DeclNode* SemanticChecker::lookup(const Node& at, const std::string& name)
{
    auto res = lookup_name(name);
    if (res.kind == LookupResult::LocalVar)
        return res.decl;
    fail(at, "variable '" + name + "' is used before its declaration");
}

void SemanticChecker::maybe_rewrite_field(std::unique_ptr<ExprNode>& slot)
{
    if (!slot) return;
    auto* v = dynamic_cast<VarNameNode*>(slot.get());
    if (!v) return;
    auto res = lookup_name(v->name);
    if (res.kind != LookupResult::FieldOfThis) return;

    auto this_node = make_unique<ThisNode>(v->line, v->col);
    this_node->type = current_struct_info->name;
    auto fa = make_unique<FieldAccessNode>(v->line, v->col, std::move(this_node), v->name);
    slot = std::move(fa);
}

void SemanticChecker::check_expr(std::unique_ptr<ExprNode>& slot)
{
    maybe_rewrite_field(slot);
    maybe_rewrite_callable_call(slot);
    slot->accept(*this);
}

// If slot is a FunctionCallNode whose name resolves to a local variable,
// rewrite it to a MemberCallNode (callable-object sugar).
void SemanticChecker::maybe_rewrite_callable_call(std::unique_ptr<ExprNode>& slot)
{
    auto* fc = dynamic_cast<FunctionCallNode*>(slot.get());
    if (!fc) return;

    // Check whether the name is a visible variable
    auto var_res = lookup_name(fc->name);
    bool is_var = (var_res.kind != LookupResult::NotFound);

    // Check whether the name is also a function
    bool is_fn = functions.count(fc->name) != 0;
    bool is_mem = current_struct_info && current_struct_info->member_fns.count(fc->name);

    if (is_var && (is_fn || is_mem))
        fail(*fc, "'" + fc->name + "' is both a variable and a function: rename one of them");

    if (!is_var) return; // ordinary function call — handled by visit_fn_call

    // Rewrite: name(args...) -> name.call(args...)
    auto obj_node = make_unique<VarNameNode>(fc->line, fc->col, fc->name);
    auto mc = make_unique<MemberCallNode>(fc->line, fc->col,
                                          std::move(obj_node), "call",
                                          std::move(fc->args));
    mc->is_callable_sugar = true;
    mc->callable_name     = fc->name;
    slot = std::move(mc);
}

// Reject a lambda-typed value at a use site where lambdas are not allowed.
void SemanticChecker::reject_lambda_value(const ExprNode& expr, const std::string& role)
{
    if (expr.type.empty()) return;
    auto it = structs.find(expr.type);
    if (it == structs.end() || !it->second.is_lambda) return;
    // Find the variable name for a better error message
    if (auto* v = dynamic_cast<const VarNameNode*>(&expr))
        fail(expr, "'" + v->name + "' is a lambda and cannot be " + role +
             ": its type has no name, so no parameter can have it");
    // For field access etc., give a simpler message
    fail(expr, "a lambda value cannot be " + role +
         ": its type has no name, so no parameter can have it");
}

// Emit a nice error when a name is used inside a lambda body but not captured.
void SemanticChecker::fail_unknown_name(const Node& at, const std::string& name)
{
    if (!lambda_hints.empty())
    {
        const auto& h = lambda_hints.back();
        // Check if the name is visible in the outer scopes
        for (auto it = h.outer_scopes.rbegin(); it != h.outer_scopes.rend(); ++it)
            if (it->count(name)) {
                fail(at, "'" + name + "' is not captured by lambda '" + h.name +
                     "': add it to the capture list, [" + name + "]");
            }
        // Check if it is a field of the outer struct
        if (h.outer_struct && h.outer_struct->field_index.count(name))
            fail(at, "'" + name + "' is not captured by lambda '" + h.name +
                 "': add it to the capture list, [" + name + "]");
    }
    fail(at, "variable '" + name + "' is used before its declaration");
}

bool SemanticChecker::is_rooted_at_this(const ExprNode& expr)
{
    if (dynamic_cast<const ThisNode*>(&expr))
        return true;
    if (auto* fa = dynamic_cast<const FieldAccessNode*>(&expr))
        return is_rooted_at_this(*fa->object);
    return false;
}

bool SemanticChecker::is_chain_mut(const ExprNode& expr)
{
    if (dynamic_cast<const ThisNode*>(&expr))
        return true;
    if (auto* v = dynamic_cast<const VarNameNode*>(&expr))
        return v->decl ? v->decl->is_mut : false;
    if (auto* fa = dynamic_cast<const FieldAccessNode*>(&expr))
    {
        if (!is_chain_mut(*fa->object)) return false;
        auto it = structs.find(fa->object->type);
        if (it != structs.end() && fa->field_index >= 0 && fa->field_index < (int)it->second.fields.size())
            return it->second.fields[fa->field_index].is_mut;
        return false;
    }
    return true; // temporary call results are mutable copies
}

bool SemanticChecker::has_const_field_link(const ExprNode& expr)
{
    if (dynamic_cast<const ThisNode*>(&expr))
        return false;
    if (dynamic_cast<const VarNameNode*>(&expr))
        return false;
    if (auto* fa = dynamic_cast<const FieldAccessNode*>(&expr))
    {
        if (has_const_field_link(*fa->object)) return true;
        auto it = structs.find(fa->object->type);
        if (it != structs.end() && fa->field_index >= 0 && fa->field_index < (int)it->second.fields.size())
            return !it->second.fields[fa->field_index].is_mut;
        return false;
    }
    return false;
}

void SemanticChecker::validate_lvalue(ExprNode& expr, const Node& at)
{
    if (auto* v = dynamic_cast<VarNameNode*>(&expr))
    {
        if (!v->decl->is_mut)
            fail(at, "cannot assign to a const variable '" + v->name + "'");
        if (structs.count(v->decl->type_name) && !all_fields_mut(v->decl->type_name))
            fail(at, "cannot assign to struct '" + v->name + "' because some fields are const");
        return;
    }

    if (auto* fa = dynamic_cast<FieldAccessNode*>(&expr))
    {
        std::vector<FieldAccessNode*> chain;
        ExprNode* curr = fa;
        while (auto* f = dynamic_cast<FieldAccessNode*>(curr))
        {
            chain.push_back(f);
            curr = f->object.get();
        }

        bool is_bare_this_field = false;
        if (dynamic_cast<ThisNode*>(curr))
        {
            if (chain.size() == 1)
                is_bare_this_field = true;
        }
        else if (auto* v = dynamic_cast<VarNameNode*>(curr))
        {
            if (!v->decl->is_mut)
                fail(at, "cannot assign to field of const variable '" + v->name + "'");
        }
        else
        {
            fail(at, "cannot assign to non-lvalue");
        }

        for (int i = (int)chain.size() - 1; i >= 1; --i)
        {
            auto* node = chain[i];
            auto it = structs.find(node->object->type);
            if (it != structs.end() && node->field_index >= 0)
            {
                const auto& finfo = it->second.fields[node->field_index];
                if (!finfo.is_mut)
                    fail(at, "cannot write through const field '" + finfo.name + "'");
            }
        }

        auto* leaf = chain.front();
        auto it = structs.find(leaf->object->type);
        if (it != structs.end() && leaf->field_index >= 0)
        {
            const auto& finfo = it->second.fields[leaf->field_index];
            if (!finfo.is_mut)
            {
                auto st_it = structs.find(leaf->object->type);
                if (is_bare_this_field && st_it != structs.end() && st_it->second.is_lambda)
                    fail(at, "cannot assign to '" + finfo.name + "': the capture '" + finfo.name + "' is const, write [mut " + finfo.name + "]");
                else if (is_bare_this_field)
                    fail(at, "cannot assign to a const field '" + finfo.name + "'");
                else
                    fail(at, "cannot write through const field '" + finfo.name + "'");
            }
            if (structs.count(finfo.type) && !all_fields_mut(finfo.type))
                fail(at, "cannot assign to struct '" + finfo.name + "' because some fields are const");
        }
        return;
    }

    fail(at, "invalid lvalue");
}

void SemanticChecker::check_assignable(const ExprNode& expr,
    const std::string& want,
    const Node& at,
    const std::string& what)
{
    const std::string& have = expr.type;
    if (want == "i32" && have == "i64")
    {
        if (auto* num = dynamic_cast<const NumberNode*>(&expr))
            fail(num->line, num->col, "constant " + num->text + " does not fit in i32");
        if (auto* init = dynamic_cast<const InitNode*>(&expr))
            if (init->items.size() == 1)
                if (auto* num2 = dynamic_cast<const NumberNode*>(init->items[0].get()))
                    fail(num2->line, num2->col, "constant " + num2->text + " does not fit in i32");
    }
    if (have == want || (have == "i32" && want == "i64")) return;
    fail(at, "cannot " + what + " of type " + want + " with a value of type " + have);
}

void SemanticChecker::check_condition(std::unique_ptr<ExprNode>& cond, const Node& at, const char* kw)
{
    check_expr(cond);
    if (cond->type != "bool")
        fail(at, std::string("the condition of '") + kw + "' must be bool, got " + cond->type);
}

bool SemanticChecker::all_fields_mut(const std::string& type_name)
{
    auto it = structs.find(type_name);
    if (it == structs.end()) return true;
    for (const auto& f : it->second.fields)
    {
        if (!f.is_mut) return false;
        if (structs.count(f.type) && !all_fields_mut(f.type)) return false;
    }
    return true;
}

void SemanticChecker::check_init_item(std::unique_ptr<ExprNode>& item, const std::string& expected_type)
{
    if (auto* inner_init = dynamic_cast<InitNode*>(item.get()))
    {
        if (is_builtin_type(expected_type))
            fail(*inner_init, "nested initializer for built-in type '" + expected_type + "'");

        auto it = structs.find(expected_type);
        if (it == structs.end())
            fail(*inner_init, "unknown type '" + expected_type + "'");

        const auto& sinfo = it->second;
        if (inner_init->items.size() != sinfo.fields.size())
            fail(*inner_init, "struct '" + expected_type + "' has " +
                 std::to_string(sinfo.fields.size()) + " field(s), got " +
                 std::to_string(inner_init->items.size()));

        for (size_t i = 0; i < sinfo.fields.size(); ++i)
            check_init_item(inner_init->items[i], sinfo.fields[i].type);

        inner_init->type = expected_type;
        return;
    }

    check_expr(item);
    check_assignable(*item, expected_type, *item, "initialise");
}

llvm::Value* SemanticChecker::visit_struct_decl(StructDeclNode& n)
{
    check_new_name(n, n.name);
    if (structs.count(n.name))
        fail(n, "struct '" + n.name + "' is already declared");

    StructInfo sinfo;
    sinfo.name = n.name;

    for (size_t i = 0; i < n.fields.size(); ++i)
    {
        const auto& f = n.fields[i];
        check_new_name(*f, f->name);
        if (sinfo.field_index.count(f->name))
            fail(*f, "field '" + f->name + "' is already declared in struct '" + n.name + "'");

        sinfo.field_index[f->name] = (int)i;
        sinfo.fields.push_back({f->type_name, f->name, f->is_mut, (int)i});
    }

    structs[n.name] = std::move(sinfo);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_fn_decl(FunctionDeclNode& n)
{
    check_new_name(n, n.name);
    FunctionDeclNode* prev_fn = current_fn;
    current_fn = &n;

    scopes.emplace_back();
    for (const auto& p : n.params)
    {
        check_new_name(*p, p->name);
        if (!is_known_type(p->type_name))
            fail(*p, "unknown type '" + p->type_name + "'");
        if (scopes.back().count(p->name))
            fail(*p, "parameter '" + p->name + "' is already declared");
        scopes.back()[p->name] = p->decl.get();
    }

    n.body->accept(*this);
    scopes.pop_back();

    current_fn = prev_fn;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_member_fn_decl(MemberFnDeclNode& n)
{
    MemberFnDeclNode* prev_mem = current_member_fn;
    current_member_fn = &n;

    scopes.emplace_back();
    for (const auto& p : n.params)
    {
        check_new_name(*p, p->name);
        if (!is_known_type(p->type_name))
            fail(*p, "unknown type '" + p->type_name + "'");
        if (scopes.back().count(p->name))
            fail(*p, "parameter '" + p->name + "' is already declared");
        scopes.back()[p->name] = p->decl.get();
    }

    n.body->accept(*this);
    scopes.pop_back();

    current_member_fn = prev_mem;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_program(ProgramNode& n)
{
    // Phase 1: Register all struct names and fields
    for (auto& s : n.structs)
    {
        for (auto& m : s->member_fns)
            m->struct_decl = s.get();
        visit_struct_decl(*s);
    }

    // Phase 2: Register member function headers and check field validity
    for (auto& s : n.structs)
    {
        current_struct_info = &structs[s->name];
        for (const auto& f : s->fields)
        {
            if (!is_known_type(f->type_name))
                fail(*f, "unknown type '" + f->type_name + "'");
        }

        for (auto& m : s->member_fns)
        {
            m->struct_decl = s.get();
            check_new_name(*m, m->name);
            if (current_struct_info->field_index.count(m->name))
                fail(*m, "'" + m->name + "' is already declared in struct '" + s->name + "'");

            if (current_struct_info->member_fns.count(m->name))
                fail(*m, "member function '" + m->name + "' is already declared in struct '" + s->name + "'");

            if (!is_known_type(m->return_type))
                fail(*m, "unknown return type '" + m->return_type + "'");

            std::set<std::string> param_names;
            for (const auto& p : m->params)
            {
                check_new_name(*p, p->name);
                if (!is_known_type(p->type_name))
                    fail(*p, "unknown type '" + p->type_name + "'");
                if (param_names.count(p->name))
                    fail(*p, "parameter '" + p->name + "' is already declared");
                param_names.insert(p->name);
            }

            current_struct_info->member_fns[m->name] = m.get();
        }
        current_struct_info = nullptr;
    }

    // Phase 3: Register global functions
    for (auto& fn : n.functions)
    {
        check_new_name(*fn, fn->name);
        if (structs.count(fn->name))
            fail(*fn, "'" + fn->name + "' is already declared");
        if (functions.count(fn->name))
            fail(*fn, "function '" + fn->name + "' is already declared");
        if (!is_known_type(fn->return_type))
            fail(*fn, "unknown return type '" + fn->return_type + "'");

        std::set<std::string> param_names;
        for (const auto& p : fn->params)
        {
            check_new_name(*p, p->name);
            if (!is_known_type(p->type_name))
                fail(*p, "unknown type '" + p->type_name + "'");
            if (param_names.count(p->name))
                fail(*p, "parameter '" + p->name + "' is already declared");
            param_names.insert(p->name);
        }

        functions[fn->name] = fn.get();
    }

    // Phase 4: Check member function bodies
    in_member_body_pass = true;
    for (auto& s : n.structs)
    {
        current_struct_node = s.get();
        current_struct_info = &structs[s->name];
        for (auto& m : s->member_fns)
        {
            current_fn = m.get();
            visit_member_fn_decl(*m);
            current_fn = nullptr;
        }
        current_struct_info = nullptr;
        current_struct_node = nullptr;
    }
    in_member_body_pass = false;

    // Phase 5: Fixed point iteration for mutating member functions
    for (auto* m : directly_mutates)
        m->mutates = true;

    bool changed = true;
    while (changed)
    {
        changed = false;
        for (auto& [caller, callees] : call_edges)
        {
            if (!caller->mutates)
            {
                for (auto* callee : callees)
                {
                    if (callee->mutates)
                    {
                        caller->mutates = true;
                        changed = true;
                        break;
                    }
                }
            }
        }
    }

    // Phase 6: Validate deferred call sites
    for (const auto& call : deferred_calls)
    {
        if (call.target->mutates)
        {
            if (call.is_sugar) {
                if (!call.chain_is_mut)
                    fail(*call.at, "cannot call '" + call.display_name + "': its 'call' changes it, and it is not mut");
            } else {
                if (call.has_const_field_link)
                    fail(*call.at, "cannot call mutating member function '" + call.target->name + "' through a const field");
                if (!call.chain_is_mut)
                    fail(*call.at, "cannot call mutating member function '" + call.target->name + "' on a const object");
            }
        }
    }

    // Phase 7: Check global function bodies, top-level statements, and exit
    for (auto& fn : n.functions)
        visit_fn_decl(*fn);

    scopes.clear();
    scopes.emplace_back();
    for (auto& s : n.stmts) s->accept(*this);
    check_expr(n.exit_stmt->expr);
    reject_lambda_value(*n.exit_stmt->expr, "returned");
    if (current_fn)
    {
        check_assignable(*n.exit_stmt->expr, current_fn->return_type, *n.exit_stmt, "return");
    }

    // Append generated lambda structs so codegen sees them
    for (auto& gs : generated_structs)
        n.structs.push_back(std::move(gs));
    generated_structs.clear();

    return nullptr;
}

llvm::Value* SemanticChecker::visit_block(BlockNode& n)
{
    scopes.emplace_back();
    for (auto& s : n.stmts) s->accept(*this);
    if (n.exit_stmt) n.exit_stmt->accept(*this);
    scopes.pop_back();
    return nullptr;
}

llvm::Value* SemanticChecker::visit_decl(DeclNode& n)
{
    check_new_name(n, n.name);
    if (scopes.back().count(n.name))
        fail(n, "variable '" + n.name + "' is already declared in this block");

    if (!is_known_type(n.type_name))
        fail(n, "unknown type '" + n.type_name + "'");

    if (is_builtin_type(n.type_name))
    {
        if (n.expr->items.size() != 1)
            fail(n, "type '" + n.type_name + "' takes 1 initialiser, got " +
                    std::to_string(n.expr->items.size()));

        if (auto* inner_init = dynamic_cast<InitNode*>(n.expr->items[0].get()))
            fail(*inner_init, "nested initializer for built-in type '" + n.type_name + "'");

        check_expr(n.expr->items[0]);
        if (!n.is_closure_init)
            reject_lambda_value(*n.expr->items[0], "used to initialise '" + n.name + "'");
        check_assignable(*n.expr->items[0], n.type_name, n, "initialise '" + n.name + "'");
        n.expr->type = n.expr->items[0]->type;
    }
    else
    {
        const auto& sinfo = structs[n.type_name];
        if (n.expr->items.size() == 1)
        {
            check_expr(n.expr->items[0]);
            if (!n.is_closure_init)
                reject_lambda_value(*n.expr->items[0], "used to initialise '" + n.name + "'");
            if (n.expr->items[0]->type == n.type_name)
            {
                n.expr->type = n.type_name;
                scopes.back()[n.name] = &n;
                return nullptr;
            }
        }

        if (n.expr->items.size() != sinfo.fields.size())
        {
            fail(n, "struct '" + n.type_name + "' has " +
                std::to_string(sinfo.fields.size()) + " field(s), got " +
                std::to_string(n.expr->items.size()));
        }

        for (size_t i = 0; i < sinfo.fields.size(); ++i)
            check_init_item(n.expr->items[i], sinfo.fields[i].type);

        n.expr->type = n.type_name;
    }

    scopes.back()[n.name] = &n;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_asmt(AsmtNode& n)
{
    check_expr(n.lhs);
    if (current_member_fn && is_rooted_at_this(*n.lhs))
        directly_mutates.insert(current_member_fn);

    validate_lvalue(*n.lhs, n);

    check_expr(n.rhs);
    reject_lambda_value(*n.rhs, "assigned");

    std::string what = "assign to '" + lhs_chain_string(n.lhs.get()) + "'";
    if (auto* fa = dynamic_cast<FieldAccessNode*>(n.lhs.get()))
        what = "assign to field '" + fa->field_name + "'";
    if (auto* v = dynamic_cast<VarNameNode*>(n.lhs.get()))
        what = "assign to '" + v->name + "'";

    check_assignable(*n.rhs, n.lhs->type, n, what);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_exit(ExitNode& n)
{
    check_expr(n.expr);
    reject_lambda_value(*n.expr, "returned");
    if (current_fn)
    {
        check_assignable(*n.expr, current_fn->return_type, n, "return");
    }
    return nullptr;
}

llvm::Value* SemanticChecker::visit_if(IfNode& n)
{
    check_condition(n.cond, n, kw::IF);
    n.then_block->accept(*this);
    if (n.else_block) n.else_block->accept(*this);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_while(WhileNode& n)
{
    check_condition(n.cond, n, kw::WHILE);
    n.body->accept(*this);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_number(NumberNode& n)
{
    errno = 0;
    char* endptr = nullptr;
    long long v = std::strtoll(n.text.c_str(), &endptr, 10);
    if (errno == ERANGE)
        fail(n, "constant " + n.text + " does not fit in i64");
    n.type  = (v >= INT32_MIN && v <= INT32_MAX) ? "i32" : "i64";
    n.value = v;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_bool(BoolNode& n)  { n.type = "bool"; return nullptr; }

llvm::Value* SemanticChecker::visit_this(ThisNode& n)
{
    if (current_struct_info)
        n.type = current_struct_info->name;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_var(VarNameNode& n)
{
    auto res = lookup_name(n.name);
    if (res.kind == LookupResult::NotFound)
        fail_unknown_name(n, n.name);
    if (res.kind == LookupResult::FieldOfThis)
        fail(n, "internal: VarNameNode for field should have been rewritten");

    n.decl = res.decl;
    n.type = res.decl->type_name;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_not(NotNode& n)
{
    check_expr(n.expr);
    if (n.expr->type != "bool")
        fail(n, "cannot apply '!' to " + n.expr->type);
    n.type = "bool";
    return nullptr;
}

llvm::Value* SemanticChecker::visit_binop(BinaryOpNode& n)
{
    check_expr(n.left);
    check_expr(n.right);
    const std::string& lt = n.left->type;
    const std::string& rt = n.right->type;

    if (n.op == "+" || n.op == "-" || n.op == "*")
    {
        if (!is_int(lt)) fail(n, "cannot apply '" + n.op + "' to " + lt);
        if (!is_int(rt)) fail(n, "cannot apply '" + n.op + "' to " + rt);
        n.type = (lt == "i64" || rt == "i64") ? "i64" : "i32";
    }
    else
    {
        bool ok = (is_int(lt) && is_int(rt)) || (lt == "bool" && rt == "bool");
        if (!ok) fail(n, "cannot compare " + lt + " with " + rt);
        n.type = "bool";
    }
    return nullptr;
}

llvm::Value* SemanticChecker::visit_fn_call(FunctionCallNode& n)
{
    //lookup_name() then lookup or in other words get the declaration of this variable with function name if it exsits
    // if it exists check if the struct of this type has member function call
    // if it does then swap it to name.call()
    // and then visit the structs memeber function call()

    if (current_struct_info)
    {
        auto mit = current_struct_info->member_fns.find(n.name);
        if (mit != current_struct_info->member_fns.end())
        {
            MemberFnDeclNode* m = mit->second;
            n.fn_decl = m;
            if (n.args.size() != m->params.size())
                fail(n, "'" + n.name + "' takes " + std::to_string(m->params.size()) +
                        " argument(s), got " + std::to_string(n.args.size()));

            for (size_t i = 0; i < n.args.size(); ++i)
            {
                check_expr(n.args[i]);
                const auto& want = m->params[i]->type_name;
                const auto& have = n.args[i]->type;
                if (have != want && !(have == "i32" && want == "i64"))
                {
                    fail(*n.args[i], "cannot pass argument " + std::to_string(i + 1) + " of '" +
                        n.name + "' of type " + want + " with a value of type " + have);
                }
            }

            n.type = m->return_type;

            if (in_member_body_pass)
            {
                call_edges[current_member_fn].insert(m);
                deferred_calls.push_back({&n, m, true, false, false, n.name});
            }
            return nullptr;
        }
    }

    auto it = functions.find(n.name);
    if (it == functions.end())
        fail(n, "function '" + n.name + "' is not declared");

    FunctionDeclNode* f = it->second;
    n.fn_decl = f;

    if (n.args.size() != f->params.size())
        fail(n, "'" + n.name + "' takes " + std::to_string(f->params.size()) +
                " argument(s), got " + std::to_string(n.args.size()));

    for (size_t i = 0; i < n.args.size(); ++i)
    {
        check_expr(n.args[i]);
        reject_lambda_value(*n.args[i], "passed to '" + n.name + "'");
        const auto& want = f->params[i]->type_name;
        const auto& have = n.args[i]->type;
        if (have != want && !(have == "i32" && want == "i64"))
        {
            fail(*n.args[i], "cannot pass argument " + std::to_string(i + 1) + " of '" +
                n.name + "' of type " + want + " with a value of type " + have);
        }
    }

    n.type = f->return_type;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_field_access(FieldAccessNode& n)
{
    check_expr(n.object);
    const auto& cur_type = n.object->type;
    auto it = structs.find(cur_type);
    if (it == structs.end())
        fail(n, "type '" + cur_type + "' is not a struct");
    if (it->second.is_lambda && !dynamic_cast<ThisNode*>(n.object.get()))
        fail(n, "'" + n.field_name + "' is a capture of a lambda and is not visible outside its body");

    const auto& sinfo = it->second;
    auto f_it = sinfo.field_index.find(n.field_name);
    if (f_it == sinfo.field_index.end())
        fail(n, "struct '" + cur_type + "' has no field '" + n.field_name + "'");

    n.field_index = f_it->second;
    n.type = sinfo.fields[n.field_index].type;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_member_call(MemberCallNode& n)
{
    check_expr(n.object);
    const auto& cur_type = n.object->type;
    auto it = structs.find(cur_type);
    if (it == structs.end())
    {
        if (n.is_callable_sugar)
            fail(n, "'" + n.callable_name + "' is not callable: it is " + cur_type);
        fail(n, "type '" + cur_type + "' is not a struct");
    }

    if (!n.is_callable_sugar)
    {
        bool has_field = it->second.field_index.count(n.fn_name) != 0;
        bool has_mfn   = it->second.member_fns.count(n.fn_name) != 0;
        if (has_field && has_mfn)
            fail(n, "'" + n.fn_name + "' is both a variable and a function: rename one of them");

        if (has_field)
        {
            int idx = it->second.field_index.at(n.fn_name);
            auto fa = make_unique<FieldAccessNode>(n.line, n.col, std::move(n.object), n.fn_name);
            fa->field_index = idx;
            fa->type = it->second.fields[idx].type;
            n.object = std::move(fa);
            n.is_callable_sugar = true;
            n.callable_name = n.fn_name;
            n.fn_name = "call";

            const auto& new_type = n.object->type;
            it = structs.find(new_type);
            if (it == structs.end())
                fail(n, "'" + n.callable_name + "' is not callable: it is " + new_type);
        }
    }

    const auto& sinfo = it->second;
    auto m_it = sinfo.member_fns.find(n.fn_name);
    if (m_it == sinfo.member_fns.end())
    {
        if (n.is_callable_sugar)
            fail(n, "'" + n.callable_name + "' is not callable: " + it->first + " has no member function 'call'");
        fail(n, "struct '" + it->first + "' has no member function '" + n.fn_name + "'");
    }

    MemberFnDeclNode* mfn = m_it->second;
    n.fn = mfn;

    std::string display = n.is_callable_sugar ? n.callable_name : n.fn_name;

    if (n.args.size() != mfn->params.size())
        fail(n, "'" + display + "' takes " + std::to_string(mfn->params.size()) +
                " argument(s), got " + std::to_string(n.args.size()));

    for (size_t i = 0; i < n.args.size(); ++i)
    {
        check_expr(n.args[i]);
        reject_lambda_value(*n.args[i], "passed to '" + display + "'");
        const auto& want = mfn->params[i]->type_name;
        const auto& have = n.args[i]->type;
        if (have != want && !(have == "i32" && want == "i64"))
        {
            fail(*n.args[i], "cannot pass argument " + std::to_string(i + 1) + " of '" +
                display + "' of type " + want + " with a value of type " + have);
        }
    }

    n.type = mfn->return_type;

    if (in_member_body_pass)
    {
        if (is_rooted_at_this(*n.object))
            call_edges[current_member_fn].insert(mfn);
        deferred_calls.push_back({&n, mfn, is_chain_mut(*n.object), has_const_field_link(*n.object),
                                   n.is_callable_sugar, display});
    }
    else
    {
        if (mfn->mutates)
        {
            if (n.is_callable_sugar) {
                if (!is_chain_mut(*n.object) || has_const_field_link(*n.object))
                    fail(n, "cannot call '" + n.callable_name + "': its 'call' changes it, and it is not mut");
            } else {
                if (has_const_field_link(*n.object))
                    fail(n, "cannot call mutating member function '" + mfn->name + "' through a const field");
                if (!is_chain_mut(*n.object))
                    fail(n, "cannot call mutating member function '" + mfn->name + "' on a const object");
            }
        }
    }
    return nullptr;
}

llvm::Value* SemanticChecker::visit_print(PrintNode& n)
{
    if (n.is_string) return nullptr;

    check_expr(n.expr);
    reject_lambda_value(*n.expr, "printed");
    const auto& t = n.expr->type;
    if (!is_builtin_type(t) && !structs.count(t))
        fail(n, "cannot print value of type '" + t + "'");
    return nullptr;
}

llvm::Value* SemanticChecker::visit_read(ReadNode& n)
{
    n.type = "i32";
    return nullptr;
}

llvm::Value* SemanticChecker::visit_lambda(LambdaNode& L)
{
    // Guard: don't lower twice (LambdaNode stays in tree)
    if (L.lowered_struct) return nullptr;

    // -------------------------------------------------------
    // 1. Resolve captures
    // -------------------------------------------------------
    std::set<std::string> seen_caps;
    bool any_mut = false;
    std::vector<std::unique_ptr<FieldDeclNode>>    field_nodes;
    std::vector<std::unique_ptr<ExprNode>>         init_items;
    std::vector<FieldInfo>                         fields;
    std::map<std::string, int>                     field_index_map;

    for (const auto& cap : L.captures)
    {
        if (seen_caps.count(cap->name))
            fail(*cap, "'" + cap->name + "' is captured twice");
        seen_caps.insert(cap->name);

        auto res = lookup_name(cap->name);
        if (res.kind == LookupResult::NotFound)
            fail(*cap, "cannot capture '" + cap->name + "': no variable '" + cap->name + "' is visible here");

        // Determine the type of the captured variable
        std::string cap_type;
        if (res.kind == LookupResult::LocalVar)
        {
            cap_type = res.decl->type_name;
        }
        else // FieldOfThis
        {
            cap_type = res.field_type;
        }

        bool effective_mut = cap->is_mut;
        any_mut = any_mut || effective_mut;

        int idx = (int)fields.size();
        field_index_map[cap->name] = idx;
        fields.push_back({cap_type, cap->name, effective_mut, idx});

        field_nodes.push_back(make_unique<FieldDeclNode>(
            cap->line, cap->col, cap_type, cap->name, effective_mut));
        init_items.push_back(make_unique<VarNameNode>(cap->line, cap->col, cap->name));
    }

    // -------------------------------------------------------
    // 2. Check signature (params and return type)
    // -------------------------------------------------------
    if (!is_known_type(L.return_type))
        fail(L, "unknown return type '" + L.return_type + "'");

    std::set<std::string> param_names;
    for (const auto& p : L.params)
    {
        if (!is_known_type(p->type_name))
            fail(*p, "unknown type '" + p->type_name + "'");
        if (param_names.count(p->name))
            fail(*p, "parameter '" + p->name + "' is already declared");
        if (seen_caps.count(p->name))
            fail(*p, "parameter '" + p->name + "' has the same name as a capture");
        param_names.insert(p->name);
    }

    // -------------------------------------------------------
    // 3. Build the generated struct
    // -------------------------------------------------------
    // Generate a unique struct name: "lambda.<name>" or "lambda.<name>.<N>"
    std::string sname = "lambda." + L.name;
    if (structs.count(sname))
    {
        sname = "lambda." + L.name + "." + std::to_string(++lambda_counter);
    }

    // Build the call member function node (take ownership of params & body from L)
    std::vector<std::unique_ptr<FuncParamNode>> call_params;
    for (auto& p : L.params)
    {
        auto new_param = make_unique<FuncParamNode>(p->line, p->col, p->type_name, p->name);
        call_params.push_back(std::move(new_param));
    }

    auto call_node_uptr = make_unique<MemberFnDeclNode>(
        L.line, L.col, "call", L.return_type,
        std::move(call_params), std::move(L.body));

    auto struct_node_uptr = make_unique<StructDeclNode>(
        L.line, L.col, sname,
        std::move(field_nodes));

    StructDeclNode* struct_node = struct_node_uptr.get();
    MemberFnDeclNode* call_node = call_node_uptr.get();

    // Add the call fn to the struct
    call_node->struct_decl = struct_node;
    struct_node->member_fns.push_back(std::move(call_node_uptr));

    // Register struct info (without call yet — added after body check)
    StructInfo sinfo;
    sinfo.name = sname;
    sinfo.is_lambda = true;
    sinfo.fields = fields;
    sinfo.field_index = field_index_map;
    structs[sname] = std::move(sinfo);

    // -------------------------------------------------------
    // 4. Check the lambda body in a fresh scope, with captures as fields
    // -------------------------------------------------------
    {
        // Save all state we will modify
        auto saved_scopes         = scopes;
        auto saved_struct_info    = current_struct_info;
        auto saved_struct_node    = current_struct_node;
        auto saved_fn             = current_fn;
        auto saved_member_fn      = current_member_fn;
        auto saved_in_member_pass = in_member_body_pass;

        // Install a fresh scope (captures arrive via 'this')
        scopes = { Scope{} };
        current_struct_info  = &structs[sname];
        current_struct_node  = struct_node;
        current_fn           = call_node;
        current_member_fn    = call_node;
        in_member_body_pass  = false; // body runs in Phase 7 context

        // Push a hint so fail_unknown_name gives helpful messages
        lambda_hints.push_back({L.name, saved_scopes, saved_struct_info});

        visit_member_fn_decl(*call_node);

        lambda_hints.pop_back();

        // Restore all state
        scopes              = std::move(saved_scopes);
        current_struct_info = saved_struct_info;
        current_struct_node = saved_struct_node;
        current_fn          = saved_fn;
        current_member_fn   = saved_member_fn;
        in_member_body_pass = saved_in_member_pass;
    }

    // -------------------------------------------------------
    // 5. Register call in member_fns (after body check)
    // -------------------------------------------------------
    structs[sname].member_fns["call"] = call_node;

    // Determine mutates: does the call body write to any capture (field)?
    // visit_member_fn_decl already called — if it mutates, directly_mutates has it
    call_node->mutates = directly_mutates.count(call_node) > 0;

    // -------------------------------------------------------
    // 6. Build the closure variable declaration
    // -------------------------------------------------------
    auto init_node_ptr = make_unique<InitNode>(L.line, L.col, std::move(init_items));
    auto decl = make_unique<DeclNode>(
        L.line, L.col, sname, L.name, any_mut, std::move(init_node_ptr));
    decl->is_closure_init = true;
    L.lowered_struct = struct_node;
    L.lowered_decl   = std::move(decl);

    // -------------------------------------------------------
    // 7. Collect the generated struct and visit the closure decl
    // -------------------------------------------------------
    generated_structs.push_back(std::move(struct_node_uptr));
    L.lowered_decl->accept(*this);

    return nullptr;
}


llvm::Value* SemanticChecker::visit_init(InitNode& n)
{
    for (auto& item : n.items)
        check_expr(item);
    if (!n.items.empty())
        n.type = n.items[0]->type;
    return nullptr;
}

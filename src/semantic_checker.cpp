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

// ---- helpers ----

void SemanticChecker::fail(int line, int col, const std::string& msg)
{
    throw std::runtime_error(
        "line " + std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
}

void SemanticChecker::fail(const Node& at, const std::string& msg)
{
    fail(at.line, at.col, msg);
}

DeclNode* SemanticChecker::lookup(const Node& at, const std::string& name)
{
    for (auto f = scopes.rbegin(); f != scopes.rend(); ++f)
    {
        auto it = f->find(name);
        if (it != f->end()) return it->second;
    }
    fail(at, "variable '" + name + "' is used before its declaration");
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

void SemanticChecker::check_condition(ExprNode& cond, const Node& at, const char* kw)
{
    cond.accept(*this);
    if (cond.type != "bool")
        fail(at, std::string("the condition of '") + kw + "' must be bool, got " + cond.type);
}

bool SemanticChecker::all_fields_mut(const std::string& type_name)
{
    auto it = structs.find(type_name);
    if (it == structs.end()) return true; // primitive type
    for (const auto& f : it->second.fields)
    {
        if (!f.is_mut) return false;
        if (!all_fields_mut(f.type)) return false;
    }
    return true;
}

void SemanticChecker::check_init_item(ExprNode& item, const std::string& expected_type)
{
    if (expected_type == "i32" || expected_type == "i64" || expected_type == "bool")
    {
        if (auto* init = dynamic_cast<InitNode*>(&item))
            fail(*init, "nested initializer for built-in type '" + expected_type + "'");
        item.accept(*this);
        check_assignable(item, expected_type, item, "initialise");
    }
    else
    {
        auto it = structs.find(expected_type);
        if (it == structs.end())
            fail(item, "unknown struct type '" + expected_type + "'");

        if (auto* init = dynamic_cast<InitNode*>(&item))
        {
            const auto& sinfo = it->second;
            if (init->items.size() != sinfo.fields.size())
                fail(*init, "struct '" + expected_type + "' has " +
                     std::to_string(sinfo.fields.size()) + " field(s), got " +
                     std::to_string(init->items.size()));
            for (size_t j = 0; j < sinfo.fields.size(); ++j)
                check_init_item(*init->items[j], sinfo.fields[j].type);
            init->type = expected_type;
        }
        else
        {
            item.accept(*this);
            if (item.type != expected_type)
                fail(item, "cannot initialise field of type " + expected_type +
                           " with value of type " + item.type);
        }
    }
}

// ---- visitors ----

llvm::Value* SemanticChecker::visit_struct_decl(StructDeclNode& n)
{
    if (structs.count(n.name) || functions.count(n.name) ||
        n.name == "i32" || n.name == "i64" || n.name == "bool")
        fail(n, "'" + n.name + "' is already declared");

    if (n.fields.empty())
        fail(n, "struct '" + n.name + "' must have at least one field");

    StructInfo sinfo;
    sinfo.name = n.name;

    for (size_t i = 0; i < n.fields.size(); ++i)
    {
        const auto& f = n.fields[i];
        if (sinfo.field_index.count(f->name))
            fail(*f, "duplicate field '" + f->name + "' in struct '" + n.name + "'");

        if (f->type_name != "i32" && f->type_name != "i64" && f->type_name != "bool" &&
            !structs.count(f->type_name))
        {
            if (f->type_name == n.name)
                fail(*f, "struct '" + n.name + "' cannot contain itself");
            fail(*f, "unknown type '" + f->type_name + "'");
        }

        sinfo.field_index[f->name] = (int)i;
        sinfo.fields.push_back({f->type_name, f->name, f->is_mut, (int)i});
    }

    structs[n.name] = std::move(sinfo);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_fn_decl(FunctionDeclNode& n)
{
    current_fn = &n;
    scopes.clear();
    scopes.emplace_back(); // isolated local scope

    for (auto& p : n.params)
    {
        if (scopes.back().count(p->name))
            fail(*p, "parameter '" + p->name + "' is already declared");

        if (p->type_name != "i32" && p->type_name != "i64" && p->type_name != "bool" &&
            !structs.count(p->type_name))
            fail(*p, "unknown parameter type '" + p->type_name + "'");

        scopes.back()[p->name] = p->decl.get();
    }

    n.body->accept(*this);
    current_fn = nullptr;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_program(ProgramNode& n)
{
    // 1. Process all structs in declaration order
    for (auto& s : n.structs)
        s->accept(*this);

    // 2. Pass 1: Collect all function signatures
    for (auto& fn : n.functions)
    {
        if (functions.count(fn->name) || structs.count(fn->name) ||
            fn->name == "i32" || fn->name == "i64" || fn->name == "bool")
            fail(*fn, "'" + fn->name + "' is already declared");

        if (fn->return_type != "i32" && fn->return_type != "i64" && fn->return_type != "bool" &&
            !structs.count(fn->return_type))
            fail(*fn, "unknown return type '" + fn->return_type + "'");

        functions[fn->name] = fn.get();
    }

    // 3. Pass 2: Check all function bodies
    for (auto& fn : n.functions)
        fn->accept(*this);

    // 4. Check top-level statements and exit
    scopes.clear();
    scopes.emplace_back(); // top-level scope
    for (auto& s : n.stmts) s->accept(*this);
    n.exit_stmt->accept(*this);
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
    if (scopes.back().count(n.name))
        fail(n, "variable '" + n.name + "' is already declared in this block");

    if (n.type_name != "i32" && n.type_name != "i64" && n.type_name != "bool" &&
        !structs.count(n.type_name))
        fail(n, "unknown type '" + n.type_name + "'");

    if (n.type_name == "i32" || n.type_name == "i64" || n.type_name == "bool")
    {
        if (n.expr->items.size() != 1)
            fail(n, "type '" + n.type_name + "' takes 1 initialiser, got " +
                    std::to_string(n.expr->items.size()));

        if (auto* inner_init = dynamic_cast<InitNode*>(n.expr->items[0].get()))
            fail(*inner_init, "nested initializer for built-in type '" + n.type_name + "'");

        n.expr->items[0]->accept(*this);
        check_assignable(*n.expr->items[0], n.type_name, n, "initialise '" + n.name + "'");
        n.expr->type = n.expr->items[0]->type;
    }
    else
    {
        const auto& sinfo = structs[n.type_name];
        // Check for 1 item copy / call of same struct type
        if (n.expr->items.size() == 1)
        {
            n.expr->items[0]->accept(*this);
            if (n.expr->items[0]->type == n.type_name)
            {
                n.expr->type = n.type_name;
                scopes.back()[n.name] = &n;
                return nullptr;
            }
        }

        if (n.expr->items.size() != sinfo.fields.size())
            fail(n, "struct '" + n.type_name + "' has " +
                 std::to_string(sinfo.fields.size()) + " field(s), got " +
                 std::to_string(n.expr->items.size()));

        for (size_t i = 0; i < sinfo.fields.size(); ++i)
            check_init_item(*n.expr->items[i], sinfo.fields[i].type);

        n.expr->type = n.type_name;
    }

    scopes.back()[n.name] = &n;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_asmt(AsmtNode& n)
{
    DeclNode* decl = lookup(n, n.name);
    n.decl = decl;

    if (n.field_names.empty())
    {
        if (!decl->is_mut)
            fail(n, "cannot assign to a const variable '" + n.name + "'");

        if (structs.count(decl->type_name) && !all_fields_mut(decl->type_name))
            fail(n, "cannot assign to struct '" + n.name + "' because some fields are const");

        n.expr->accept(*this);
        check_assignable(*n.expr, decl->type_name, n, "assign to '" + n.name + "'");
    }
    else
    {
        if (!decl->is_mut)
            fail(n, "cannot assign to field of const variable '" + n.name + "'");

        std::string cur_type = decl->type_name;
        n.path.clear();

        for (size_t i = 0; i < n.field_names.size(); ++i)
        {
            const auto& fn = n.field_names[i];
            auto it = structs.find(cur_type);
            if (it == structs.end())
                fail(n, "type '" + cur_type + "' is not a struct");

            const auto& sinfo = it->second;
            auto f_it = sinfo.field_index.find(fn);
            if (f_it == sinfo.field_index.end())
                fail(n, "struct '" + cur_type + "' has no field '" + fn + "'");

            const auto& finfo = sinfo.fields[f_it->second];
            if (!finfo.is_mut)
                fail(n, "cannot write through const field '" + fn + "'");

            n.path.push_back(finfo.index);
            cur_type = finfo.type;
        }

        n.expr->accept(*this);
        check_assignable(*n.expr, cur_type, n, "assign to field '" + n.field_names.back() + "'");
    }
    return nullptr;
}

llvm::Value* SemanticChecker::visit_exit(ExitNode& n)
{
    n.expr->accept(*this);
    if (current_fn)
    {
        check_assignable(*n.expr, current_fn->return_type, n, "return");
    }
    else
    {
        if (n.expr->type != "i32" && n.expr->type != "i64" && n.expr->type != "bool")
            fail(n, "top-level exit must have a built-in type, got " + n.expr->type);
    }
    return nullptr;
}

llvm::Value* SemanticChecker::visit_if(IfNode& n)
{
    check_condition(*n.cond, n, kw::IF);
    n.then_block->accept(*this);
    if (n.else_block) n.else_block->accept(*this);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_while(WhileNode& n)
{
    check_condition(*n.cond, n, kw::WHILE);
    n.body->accept(*this);
    return nullptr;
}

llvm::Value* SemanticChecker::visit_number(NumberNode& n)
{
    errno = 0;
    char* end = nullptr;
    long long v = std::strtoll(n.text.c_str(), &end, 10);
    if (errno == ERANGE)
        fail(n, "constant " + n.text + " does not fit in i64");
    n.type  = (v >= INT32_MIN && v <= INT32_MAX) ? "i32" : "i64";
    n.value = v;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_bool(BoolNode& n)  { n.type = "bool"; return nullptr; }

llvm::Value* SemanticChecker::visit_var(VarNameNode& n)
{
    n.decl = lookup(n, n.name);
    n.type = n.decl->type_name;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_not(NotNode& n)
{
    n.expr->accept(*this);
    if (n.expr->type != "bool")
        fail(n, "cannot apply '!' to " + n.expr->type);
    n.type = "bool";
    return nullptr;
}

llvm::Value* SemanticChecker::visit_binop(BinaryOpNode& n)
{
    n.left->accept(*this);
    n.right->accept(*this);
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
        n.args[i]->accept(*this);
        const auto& want = f->params[i]->type_name;
        const auto& have = n.args[i]->type;
        if (have != want && !(have == "i32" && want == "i64"))
            fail(*n.args[i], "cannot pass argument " + std::to_string(i + 1) + " of '" +
                 n.name + "' of type " + want + " with a value of type " + have);
    }

    n.type = f->return_type;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_field_access(StructFieldAccessNode& n)
{
    DeclNode* decl = lookup(n, n.base_name);
    n.base_decl = decl;

    std::string cur_type = decl->type_name;
    n.path.clear();

    for (const auto& fn : n.field_names)
    {
        auto it = structs.find(cur_type);
        if (it == structs.end())
            fail(n, "type '" + cur_type + "' is not a struct");

        const auto& sinfo = it->second;
        auto f_it = sinfo.field_index.find(fn);
        if (f_it == sinfo.field_index.end())
            fail(n, "struct '" + cur_type + "' has no field '" + fn + "'");

        const auto& finfo = sinfo.fields[f_it->second];
        n.path.push_back(finfo.index);
        cur_type = finfo.type;
    }

    n.type = cur_type;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_init(InitNode& n)
{
    for (auto& item : n.items)
        item->accept(*this);
    if (!n.items.empty())
        n.type = n.items[0]->type;
    return nullptr;
}

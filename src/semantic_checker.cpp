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
        if (auto* num = dynamic_cast<const NumberNode*>(&expr))
            fail(num->line, num->col, "constant " + num->text + " does not fit in i32");
    if (have == want || (have == "i32" && want == "i64")) return;
    fail(at, "cannot " + what + " of type " + want + " with a value of type " + have);
}

void SemanticChecker::check_condition(ExprNode& cond, const Node& at, const char* kw)
{
    cond.accept(*this);
    if (cond.type != "bool")
        fail(at, std::string("the condition of '") + kw + "' must be bool, got " + cond.type);
}

// ---- visitors ----

llvm::Value* SemanticChecker::visit_program(ProgramNode& n)
{
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
    n.expr->accept(*this);
    check_assignable(*n.expr, n.type_name, n, "initialise '" + n.name + "'");
    scopes.back()[n.name] = &n;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_asmt(AsmtNode& n)
{
    DeclNode* decl = lookup(n, n.name);
    if (!decl->is_mut)
        fail(n, "cannot assign to a const variable '" + n.name + "'");
    n.expr->accept(*this);
    check_assignable(*n.expr, decl->type_name, n, "assign to '" + n.name + "'");
    n.decl = decl;
    return nullptr;
}

llvm::Value* SemanticChecker::visit_exit(ExitNode& n)
{
    n.expr->accept(*this); return nullptr;
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

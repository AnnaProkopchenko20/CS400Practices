#pragma once
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include "ast.h"

class SemanticChecker : public Visitor {
    // stack of frames: back() is the innermost scope
    std::vector<std::map<std::string, DeclNode*>> scopes;

    [[noreturn]] void fail(int line, int col, const std::string& msg) {
        throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }

    static bool is_int(const std::string& t) { return t == "i32" || t == "i64"; }

    // walk the frames from the top down, first hit wins
    DeclNode* lookup(const Node& at, const std::string& name)
    {
        for (auto frame = scopes.rbegin(); frame != scopes.rend(); ++frame)
        {
            auto it = frame->find(name);
            if (it != frame->end())
                return it->second;
        }
        fail(at.line, at.col, "variable '" + name + "' is used before its declaration");
    }

    void check_assignable(const ExprNode& expr, const std::string& want, const Node& at, const std::string& what)
    {
        const std::string& have = expr.type;

        // a bare constant too big for i32: blame the constant, not the name
        if (want == "i32" && have == "i64")
            if (auto* num = dynamic_cast<const NumberNode*>(&expr))
                fail(num->line, num->col, "constant " + num->text + " does not fit in i32");

        if (have == want || (have == "i32" && want == "i64"))
            return;

        fail(at.line, at.col,"cannot " + what + " of type " + want + " with a value of type " + have);
    }

public:
    SemanticChecker() { scopes.emplace_back(); }   // the top-level frame

    llvm::Value* visit_program(ProgramNode& n) override
    {
        for (auto& s : n.stmts)
        {
            s->accept(*this);
        }

        n.exit_stmt->accept(*this);

        return nullptr;
    }

    llvm::Value* visit_decl(DeclNode& n) override {
        // only the top frame counts as a duplicate; outer names may be shadowed
        if (scopes.back().count(n.name))
            fail(n.line, n.col, "variable '" + n.name + "' is already declared in this block");

        n.expr->accept(*this);
        check_assignable(*n.expr, n.type_name, n, "initialise '" + n.name + "'");

        scopes.back()[n.name] = &n;

        return nullptr;
    }

    llvm::Value* visit_asmt(AsmtNode& n) override {
        DeclNode* decl = lookup(n, n.name);

        if (!decl->is_mut)
            fail(n.line, n.col, "cannot assign to a const variable '" + n.name + "'");

        n.expr->accept(*this);
        check_assignable(*n.expr, decl->type_name, n, "assign to '" + n.name + "'");
        n.decl = decl;

        return nullptr;
    }

    llvm::Value* visit_exit(ExitNode& n) override {
        n.expr->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_number(NumberNode& n) override {
        errno = 0;
        char* end = nullptr;
        long long v = std::strtoll(n.text.c_str(), &end, 10);

        if (errno == ERANGE)
            fail(n.line, n.col, "constant " + n.text + " does not fit in i64");

        if (v >= INT32_MIN && v <= INT32_MAX)
            n.type = "i32";
        else
            n.type = "i64";

        n.value = v;

        return nullptr;
    }

    llvm::Value* visit_bool(BoolNode& n) override {
        n.type = "bool";
        return nullptr;
    }

    llvm::Value* visit_var(VarNameNode& n) override {
        n.decl = lookup(n, n.name);
        n.type = n.decl->type_name;

        return nullptr;
    }

    llvm::Value* visit_binop(BinaryOpNode& n) override {
        n.left->accept(*this);
        n.right->accept(*this);

        const std::string& lt = n.left->type;
        const std::string& rt = n.right->type;

        if (n.op == "+" || n.op == "-" || n.op == "*") {

            if (!is_int(lt))
                fail(n.line, n.col, "cannot apply '" + n.op + "' to " + lt);

            if (!is_int(rt))
                fail(n.line, n.col, "cannot apply '" + n.op + "' to " + rt);

            n.type = (lt == "i64" || rt == "i64") ? "i64" : "i32";
        }
        else
        {
            bool is_type_match = (is_int(lt) && is_int(rt)) || (lt == "bool" && rt == "bool");

            if (!is_type_match)
                fail(n.line, n.col, "cannot compare " + lt + " with " + rt);

            n.type = "bool";
        }

        return nullptr;
    }

    llvm::Value* visit_if(IfNode& n) override {
        n.cond->accept(*this);

        if (n.cond->type != "bool")
            fail(n.line, n.col, "the condition of 'if' must be bool, got " + n.cond->type);

        n.then_block->accept(*this);
        if (n.else_block)
            n.else_block->accept(*this);

        return nullptr;
    }

    llvm::Value* visit_block(BlockNode& n) override {
        scopes.emplace_back();

        for (auto& s : n.stmts)
        {
            s->accept(*this);
        }
        if (n.exit_stmt)
            n.exit_stmt->accept(*this);

        scopes.pop_back();

        return nullptr;
    }

    llvm::Value* visit_not(NotNode& n) override {
        n.expr->accept(*this);

        if (n.expr->type != "bool")
            fail(n.line, n.col, "cannot apply '!' to " + n.expr->type);

        n.type = "bool";

        return nullptr;
    }

    llvm::Value* visit_while(WhileNode& n) override {
        n.cond->accept(*this);

        if (n.cond->type != "bool")
            fail(n.line, n.col, "the condition of 'while' must be bool, got " + n.cond->type);

        n.body->accept(*this);

        return nullptr;
    }
};

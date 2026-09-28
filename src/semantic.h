#pragma once
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include "ast.h"

class SemanticChecker : public Visitor {
    std::map<std::string, DeclNode*> symbols;

    [[noreturn]] void fail(int line, int col, const std::string& msg) {
        throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col) + ": " + msg);
    }

    static bool is_int(const std::string& t) { return t == "i32" || t == "i64"; }

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
        if (symbols.count(n.name))
            fail(n.line, n.col, "variable '" + n.name + "' is already defined");

        n.expr->accept(*this);
        check_assignable(*n.expr, n.type_name, n, "initialise '" + n.name + "'");

        symbols[n.name] = &n;

        return nullptr;
    }

    llvm::Value* visit_asmt(AsmtNode& n) override {
        auto it = symbols.find(n.name);

        if (it == symbols.end())
            fail(n.line, n.col, "assignment to undeclared variable '" + n.name + "'");

        DeclNode* decl = it->second;

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
        auto it = symbols.find(n.name);

        if (it == symbols.end())
            fail(n.line, n.col, "unknown variable '" + n.name + "'");

        n.decl = it->second;
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
};
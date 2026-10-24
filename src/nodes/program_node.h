#pragma once
#include <vector>
#include <memory>
#include <iostream>
#include "node.h"
#include "stmt_node.h"
#include "exit_node.h"
#include "struct_decl_node.h"
#include "fn_decl_node.h"

struct ProgramNode : Node {
    std::vector<std::unique_ptr<StructDeclNode>>   structs;
    std::vector<std::unique_ptr<FunctionDeclNode>> functions;
    std::vector<std::unique_ptr<StmtNode>>         stmts;
    std::unique_ptr<ExitNode>                      exit_stmt;

    ProgramNode(int l, int c,
        std::vector<std::unique_ptr<StructDeclNode>> s_decls,
        std::vector<std::unique_ptr<FunctionDeclNode>> f_decls,
        std::vector<std::unique_ptr<StmtNode>> s,
        std::unique_ptr<ExitNode> e)
        : Node(l, c)
        , structs(std::move(s_decls))
        , functions(std::move(f_decls))
        , stmts(std::move(s))
        , exit_stmt(std::move(e)) {}

    ProgramNode(int l, int c,
                std::vector<std::unique_ptr<StmtNode>> s,
                std::unique_ptr<ExitNode> e)
        : Node(l, c), stmts(std::move(s)), exit_stmt(std::move(e)) {}

    void dump(int d) const override {
        indent(d); std::cout << "Program\n";
        for (auto& st : structs)   { st->dump(d + 1); }
        for (auto& fn : functions) { fn->dump(d + 1); }
        for (auto& s : stmts)      { s->dump(d + 1); }
        exit_stmt->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_program(*this); }
};

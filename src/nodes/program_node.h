#pragma once
#include "node.h"
#include "stmt_node.h"
#include "exit_node.h"

// program ::= { statement } exit_stmt
//
// The mandatory top-level exit_stmt is a separate field (not in stmts), which
// structurally enforces that the program must end with exit.
struct ProgramNode : Node {
    std::vector<std::unique_ptr<StructDeclNode>> structs;
    std::vector<std::unique_ptr<FunctionDeclNode>> functions;
    vector<unique_ptr<StmtNode>> stmts;
    unique_ptr<ExitNode>         exit_stmt;   // always non-null after a successful parse

    ProgramNode(int l, int c,
                vector<unique_ptr<StmtNode>> s,
                unique_ptr<ExitNode> e)
        : Node(l, c), stmts(std::move(s)), exit_stmt(std::move(e)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Program\n";
        for (auto& s : stmts) { s->dump(d + 1); }
        exit_stmt->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_program(*this); }
};

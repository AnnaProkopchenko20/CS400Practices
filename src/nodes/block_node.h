#pragma once
#include "node.h"
#include "stmt_node.h"
#include "exit_node.h"

struct BlockNode : Node {
    vector<unique_ptr<StmtNode>> stmts;
    unique_ptr<ExitNode>         exit_stmt;

    BlockNode(int l, int c, vector<unique_ptr<StmtNode>> s, unique_ptr<ExitNode> e)
        : Node(l, c), stmts(std::move(s)), exit_stmt(std::move(e)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Block\n";
        for (auto& s : stmts) s->dump(d + 1);
        if (exit_stmt) exit_stmt->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_block(*this); }
};

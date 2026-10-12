#pragma once
#include "node.h"
#include "stmt_node.h"
#include "exit_node.h"

// block ::= "{" NL { statement } [ exit_stmt NL ] "}" NL
//
// Practice 5 blocks carry an optional exit_stmt that -- when present --
// terminates the block.  This is a key difference from Project Stage 1,
// where exit is a top-level statement only.
struct BlockNode : Node {
    vector<unique_ptr<StmtNode>> stmts;
    unique_ptr<ExitNode>         exit_stmt;   // may be nullptr

    BlockNode(int l, int c,
              vector<unique_ptr<StmtNode>> s,
              unique_ptr<ExitNode> e = nullptr)
        : Node(l, c), stmts(std::move(s)), exit_stmt(std::move(e)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Block\n";
        for (auto& s : stmts) { s->dump(d + 1); }
        if (exit_stmt) { exit_stmt->dump(d + 1); }
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_block(*this); }
};

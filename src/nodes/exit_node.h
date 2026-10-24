#pragma once
#include "node.h"
#include "expr_node.h"

struct ExitNode : Node {
    unique_ptr<ExprNode> expr;

    ExitNode(int l, int c, unique_ptr<ExprNode> e) : Node(l, c), expr(std::move(e)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Exit\n";
        expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_exit(*this); }
};

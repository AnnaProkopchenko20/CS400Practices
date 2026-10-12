#pragma once
#include "expr_node.h"

struct NotNode : ExprNode {
    unique_ptr<ExprNode> expr;

    NotNode(int l, int c, unique_ptr<ExprNode> e)
        : ExprNode(l, c), expr(std::move(e)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Not\n";
        expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_not(*this); }
};

#pragma once
#include "expr_node.h"

struct BinaryOpNode : ExprNode {
    string               op;    // "+", "-", "*", "==" or "!="
    unique_ptr<ExprNode> left, right;

    BinaryOpNode(int l, int c, string o,
                 unique_ptr<ExprNode> lhs, unique_ptr<ExprNode> rhs)
        : ExprNode(l, c), op(std::move(o)),
          left(std::move(lhs)), right(std::move(rhs)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "BinOp " << op << "\n";
        left->dump(d + 1); right->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_binop(*this); }
};

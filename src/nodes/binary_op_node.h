#pragma once
#include "expr_node.h"

struct BinaryOpNode : ExprNode {
    string               op;
    unique_ptr<ExprNode> left;
    unique_ptr<ExprNode> right;

    BinaryOpNode(int l, int c, string op, unique_ptr<ExprNode> lhs, unique_ptr<ExprNode> rhs)
        : ExprNode(l, c), op(std::move(op)), left(std::move(lhs)), right(std::move(rhs)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "BinOp " << op << "\n";
        left->dump(d + 1);
        right->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_binop(*this); }
};

#pragma once
#include "stmt_node.h"
#include "expr_node.h"
#include "block_node.h"

struct IfNode : StmtNode {
    unique_ptr<ExprNode>  cond;
    unique_ptr<BlockNode> then_block;
    unique_ptr<BlockNode> else_block;   // may be nullptr

    IfNode(int l, int c,
           unique_ptr<ExprNode>  cond,
           unique_ptr<BlockNode> then_b,
           unique_ptr<BlockNode> else_b = nullptr)
        : StmtNode(l, c), cond(std::move(cond)),
          then_block(std::move(then_b)), else_block(std::move(else_b)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "If\n";
        cond->dump(d + 1);
        then_block->dump(d + 1);
        if (else_block) { else_block->dump(d + 1); }
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_if(*this); }
};

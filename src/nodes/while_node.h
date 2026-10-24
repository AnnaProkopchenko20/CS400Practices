#pragma once
#include "stmt_node.h"
#include "expr_node.h"
#include "block_node.h"

struct WhileNode : StmtNode {
    unique_ptr<ExprNode>  cond;
    unique_ptr<BlockNode> body;

    WhileNode(int l, int c, unique_ptr<ExprNode> cond, unique_ptr<BlockNode> body)
        : StmtNode(l, c), cond(std::move(cond)), body(std::move(body)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "While\n";
        cond->dump(d + 1);
        body->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_while(*this); }
};

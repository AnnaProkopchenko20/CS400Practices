#pragma once
#include "node.h"
#include "expr_node.h"

// exit_stmt ::= "exit" factor
//
// ExitNode is NOT a StmtNode: it is held as a separate mandatory field on
// ProgramNode and an optional field on BlockNode, never inside the stmts
// vector.  This preserves the grammar constraint that exit terminates a scope.
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

#pragma once
#include "expr_node.h"
#include "stmt_node.h"

struct AsmtNode : StmtNode {
    string               name;
    unique_ptr<ExprNode> expr;
    DeclNode*            decl = nullptr;   // back-link set by SemanticChecker (non-owning)

    AsmtNode(int l, int c, string n, unique_ptr<ExprNode> e)
        : StmtNode(l, c), name(std::move(n)), expr(std::move(e)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Assign " << name << "\n";
        expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_asmt(*this); }
};

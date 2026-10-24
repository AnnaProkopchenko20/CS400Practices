#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "expr_node.h"
#include "stmt_node.h"

struct DeclNode;

struct AsmtNode : StmtNode {
    std::string              name;
    std::vector<std::string> field_names;
    std::vector<int>         path;
    std::unique_ptr<ExprNode> expr;
    DeclNode*                decl = nullptr;

    AsmtNode(int l, int c, std::string n, std::unique_ptr<ExprNode> e)
        : StmtNode(l, c), name(std::move(n)), expr(std::move(e)) {}

    AsmtNode(int l, int c, std::string n, std::vector<std::string> f, std::unique_ptr<ExprNode> e)
        : StmtNode(l, c), name(std::move(n)), field_names(std::move(f)), expr(std::move(e)) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "Assign " << name;
        for (const auto& fn : field_names) std::cout << "." << fn;
        std::cout << "\n";
        expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_asmt(*this); }
};

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "expr_node.h"
#include "../visitor.h"

struct FunctionDeclNode;

struct FunctionCallNode : ValueNode {
    std::string name;
    std::vector<std::unique_ptr<ExprNode>> args;
    FunctionDeclNode* fn_decl = nullptr;

    FunctionCallNode(int l, int c, std::string n, std::vector<std::unique_ptr<ExprNode>> a)
        : ValueNode(l, c), name(std::move(n)), args(std::move(a)) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "Call " << name << "\n";
        for (const auto& a : args) a->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_fn_call(*this); }
};

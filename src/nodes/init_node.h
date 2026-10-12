#pragma once
#include <vector>
#include <memory>
#include <iostream>
#include "expr_node.h"
#include "../visitor.h"

struct InitNode : ExprNode {
    std::vector<std::unique_ptr<ExprNode>> items;

    InitNode(int l, int c, std::vector<std::unique_ptr<ExprNode>> it)
        : ExprNode(l, c), items(std::move(it)) {}

    void dump(int d) const override {
        for (const auto& it : items) {
            it->dump(d);
        }
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_init(*this); }
};

#pragma once
#include "value_node.h"

struct NumberNode : ValueNode {
    string    text;
    long long value = 0;

    NumberNode(int l, int c, string text)
        : ValueNode(l, c), text(std::move(text)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Const " << text << "\n";
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_number(*this); }
};

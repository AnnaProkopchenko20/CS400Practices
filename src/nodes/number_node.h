#pragma once
#include "value_node.h"

struct NumberNode : ValueNode {
    string    text;        // digit string exactly as written in the source
    long long value = 0;  // filled in by SemanticChecker

    NumberNode(int l, int c, string t) : ValueNode(l, c), text(std::move(t)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Const " << text << "\n";
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_number(*this); }
};

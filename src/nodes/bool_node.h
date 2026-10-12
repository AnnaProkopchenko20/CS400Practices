#pragma once
#include "value_node.h"

struct BoolNode : ValueNode {
    bool value;

    BoolNode(int l, int c, bool val)
        : ValueNode(l, c), value(val) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Bool " << (value ? "true" : "false") << "\n";
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_bool(*this); }
};

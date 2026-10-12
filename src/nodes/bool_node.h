#pragma once
#include "value_node.h"

// Literal true / false.
struct BoolNode : ValueNode {
    bool value;
    BoolNode(int l, int c, bool v) : ValueNode(l, c), value(v) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Bool " << (value ? "true" : "false") << "\n";
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_bool(*this); }
};

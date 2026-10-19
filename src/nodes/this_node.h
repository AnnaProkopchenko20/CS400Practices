#pragma once
#include <iostream>
#include "value_node.h"
#include "../visitor.h"

// Represents the implicit 'this' pointer inside a member function body.
// Injected by the semantic checker when a bare identifier resolves to a field.
struct ThisNode : ValueNode {
    ThisNode(int l, int c) : ValueNode(l, c) {}

    void dump(int d) const override
    {
        indent(d);
        std::cout << "This\n";
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_this(*this); }
};

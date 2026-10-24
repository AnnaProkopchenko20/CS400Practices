#pragma once
#include <iostream>
#include "value_node.h"
#include "../keywords.h"
#include "../visitor.h"

struct ReadNode : ValueNode {
    ReadNode(int l, int c) : ValueNode(l, c) {
        type = kw::I32;
    }

    void dump(int d) const override {
        indent(d);
        std::cout << "Read\n";
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_read(*this); }
};

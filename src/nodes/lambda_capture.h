#pragma once
#include <iostream>
#include <string>
#include "node.h"

// A single entry in a lambda capture list: [name] or [mut name].
struct LambdaCapture : Node {
    std::string name;
    bool        is_mut;

    LambdaCapture(int l, int c, std::string n, bool m)
        : Node(l, c), name(std::move(n)), is_mut(m) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "Capture " << (is_mut ? "mut " : "") << name << "\n";
    }

    // Captures are not independently visited; they are processed in visit_lambda.
    llvm::Value* accept(Visitor&) override { return nullptr; }
};

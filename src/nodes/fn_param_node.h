#pragma once
#include <string>
#include <iostream>
#include "node.h"

struct FuncParamNode : Node {
    std::string type_name;
    std::string name;
    bool        is_mut = false; // Parameters are copies and cannot be changed in the body

    FuncParamNode(int l, int c, std::string t, std::string n)
        : Node(l, c), type_name(std::move(t)), name(std::move(n)), is_mut(false) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "Param " << name << " " << type_name << "\n";
    }

    llvm::Value* accept(Visitor&) override { return nullptr; }
};

#pragma once
#include <string>
#include <iostream>
#include "node.h"

struct FieldDeclNode : Node {
    std::string type_name;
    std::string name;
    bool        is_mut;

    FieldDeclNode(int l, int c, std::string t, std::string n, bool m)
        : Node(l, c), type_name(std::move(t)), name(std::move(n)), is_mut(m) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "Member " << name << " " << type_name << (is_mut ? " mut" : " const") << "\n";
    }

    llvm::Value* accept(Visitor&) override { return nullptr; }
};

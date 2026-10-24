#pragma once
#include "value_node.h"

struct DeclNode;

struct VarNameNode : ValueNode {
    string    name;
    DeclNode* decl = nullptr;

    VarNameNode(int l, int c, string name)
        : ValueNode(l, c), name(std::move(name)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Var " << name << "\n";
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_var(*this); }
};

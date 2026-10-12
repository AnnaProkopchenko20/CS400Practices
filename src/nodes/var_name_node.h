#pragma once
#include "value_node.h"

struct VarNameNode : ValueNode {
    string    name;
    DeclNode* decl = nullptr;   // back-link set by SemanticChecker (non-owning)

    VarNameNode(int l, int c, string n) : ValueNode(l, c), name(std::move(n)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Var " << name << "\n";
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_var(*this); }
};

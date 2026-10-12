#pragma once
#include <string>
#include <vector>
#include <iostream>
#include "expr_node.h"
#include "../visitor.h"

struct DeclNode;

struct StructFieldAccessNode : ValueNode {
    std::string base_name;
    std::vector<std::string> field_names;
    std::vector<int> path;       // GEP indices filled by SemanticChecker
    DeclNode* base_decl = nullptr;

    StructFieldAccessNode(int l, int c, std::string base, std::vector<std::string> fields)
        : ValueNode(l, c), base_name(std::move(base)), field_names(std::move(fields)) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "FieldAccess " << base_name;
        for (const auto& f : field_names) std::cout << "." << f;
        std::cout << "\n";
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_field_access(*this); }
};

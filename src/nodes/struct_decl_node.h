#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "node.h"
#include "field_decl_node.h"
#include "../visitor.h"

struct StructDeclNode : Node {
    std::string name;
    std::vector<std::unique_ptr<FieldDeclNode>> fields;

    StructDeclNode(int l, int c, std::string n, std::vector<std::unique_ptr<FieldDeclNode>> f)
        : Node(l, c), name(std::move(n)), fields(std::move(f)) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "Struct " << name << "\n";
        for (const auto& f : fields) f->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_struct_decl(*this); }
};

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "node.h"
#include "field_decl_node.h"
#include "member_fn_decl_node.h"
#include "../visitor.h"

struct StructDeclNode : Node {
    std::string name;
    std::vector<std::unique_ptr<FieldDeclNode>> fields;
    std::vector<std::unique_ptr<MemberFnDeclNode>> member_fns;

    StructDeclNode(int l, int c, std::string n,
                   std::vector<std::unique_ptr<FieldDeclNode>> f,
                   std::vector<std::unique_ptr<MemberFnDeclNode>> m = {})
        : Node(l, c), name(std::move(n)), fields(std::move(f)), member_fns(std::move(m)) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "Struct " << name << "\n";
        for (const auto& f : fields) f->dump(d + 1);
        for (const auto& m : member_fns) m->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_struct_decl(*this); }
};

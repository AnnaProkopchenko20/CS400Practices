#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "fn_decl_node.h"
#include "../visitor.h"

struct StructDeclNode;

struct MemberFnDeclNode : FunctionDeclNode {
    StructDeclNode* struct_decl = nullptr;
    bool mutates = false;

    MemberFnDeclNode(int l, int c, std::string n, std::string ret,
                     std::vector<std::unique_ptr<FuncParamNode>> p,
                     std::unique_ptr<BlockNode> b,
                     StructDeclNode* parent = nullptr)
        : FunctionDeclNode(l, c, std::move(n), std::move(ret), std::move(p), std::move(b))
        , struct_decl(parent) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "MemberFn " << name << " -> " << return_type << "\n";
        for (const auto& p : params) p->dump(d + 1);
        if (body) body->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_member_fn_decl(*this); }
};

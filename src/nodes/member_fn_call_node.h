#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "value_node.h"
#include "expr_node.h"
#include "../visitor.h"

struct MemberFnDeclNode;

struct MemberCallNode : ValueNode {
    std::unique_ptr<ExprNode> object;
    std::string fn_name;
    std::vector<std::unique_ptr<ExprNode>> args;
    MemberFnDeclNode* fn = nullptr;

    MemberCallNode(int l, int c, std::unique_ptr<ExprNode> obj, std::string name,
                   std::vector<std::unique_ptr<ExprNode>> a)
        : ValueNode(l, c), object(std::move(obj)), fn_name(std::move(name)), args(std::move(a)) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "MemberCall " << fn_name << "\n";
        if (object) object->dump(d + 1);
        for (const auto& a : args) a->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_member_call(*this); }
};

using MemberFnCallNode = MemberCallNode;

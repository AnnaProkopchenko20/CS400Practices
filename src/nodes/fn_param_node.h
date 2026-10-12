#pragma once
#include <string>
#include <memory>
#include <iostream>
#include "node.h"
#include "decl_node.h"

struct FuncParamNode : Node {
    std::string type_name;
    std::string name;
    bool        is_mut = false;
    std::unique_ptr<DeclNode> decl;

    FuncParamNode(int l, int c, std::string t, std::string n)
        : Node(l, c)
        , type_name(std::move(t))
        , name(std::move(n))
        , is_mut(false)
        , decl(std::make_unique<DeclNode>(l, c, type_name, name, false, std::unique_ptr<InitNode>(nullptr))) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "Param " << name << " " << type_name << "\n";
    }

    llvm::Value* accept(Visitor&) override { return nullptr; }
};

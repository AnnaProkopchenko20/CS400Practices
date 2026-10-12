#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "node.h"
#include "fn_param_node.h"
#include "block_node.h"
#include "../visitor.h"

struct FunctionDeclNode : Node {
    std::string name;
    std::string return_type;
    std::vector<std::unique_ptr<FuncParamNode>> params;
    std::unique_ptr<BlockNode> body;

    FunctionDeclNode(int l, int c, std::string n, std::string ret,
                     std::vector<std::unique_ptr<FuncParamNode>> p,
                     std::unique_ptr<BlockNode> b)
        : Node(l, c), name(std::move(n)), return_type(std::move(ret)),
          params(std::move(p)), body(std::move(b)) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "Fn " << name << " -> " << return_type << "\n";
        for (const auto& p : params) p->dump(d + 1);
        body->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_fn_decl(*this); }
};

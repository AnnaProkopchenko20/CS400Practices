#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include "stmt_node.h"
#include "expr_node.h"
#include "../visitor.h"

struct PrintNode : StmtNode {
    std::unique_ptr<ExprNode> expr;
    std::string string_literal;
    bool is_string = false;

    PrintNode(int l, int c, std::unique_ptr<ExprNode> e)
        : StmtNode(l, c), expr(std::move(e)), is_string(false) {}

    PrintNode(int l, int c, std::string s)
        : StmtNode(l, c), string_literal(std::move(s)), is_string(true) {}

    void dump(int d) const override {
        indent(d);
        if (is_string) {
            std::cout << "Print \"" << string_literal << "\"\n";
        } else {
            std::cout << "Print\n";
            if (expr) expr->dump(d + 1);
        }
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_print(*this); }
};

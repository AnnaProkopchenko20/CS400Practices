#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include "init_node.h"
#include "stmt_node.h"
#include "../keywords.h"

struct DeclNode : StmtNode {
    std::string type_name;
    std::string name;
    bool        is_mut;
    std::unique_ptr<InitNode> expr;

    DeclNode(int l, int c, std::string t, std::string n, bool m, std::unique_ptr<InitNode> e)
        : StmtNode(l, c)
        , type_name(std::move(t))
        , name(std::move(n))
        , is_mut(m)
        , expr(std::move(e)) {}

    DeclNode(int l, int c, std::string t, std::string n, bool m, std::unique_ptr<ExprNode> e)
        : StmtNode(l, c)
        , type_name(std::move(t))
        , name(std::move(n))
        , is_mut(m)
    {
        std::vector<std::unique_ptr<ExprNode>> items;
        items.push_back(std::move(e));
        expr = std::make_unique<InitNode>(l, c, std::move(items));
    }

    void dump(int d) const override
    {
        indent(d);
        std::cout << "Decl " << name << " " << kw::type_keyword(type_name)
            << " " << (is_mut ? kw::MUT : kw::IMMUTABLE_FLAG) << "\n";
        expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_decl(*this); }
};

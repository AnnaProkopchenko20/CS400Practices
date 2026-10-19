#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include "value_node.h"
#include "expr_node.h"
#include "var_name_node.h"
#include "../visitor.h"

struct FieldAccessNode : ValueNode {
    std::unique_ptr<ExprNode> object;
    std::string field_name;
    int field_index = -1;

    FieldAccessNode(int l, int c, std::unique_ptr<ExprNode> obj, std::string field)
        : ValueNode(l, c), object(std::move(obj)), field_name(std::move(field)) {}

    std::string chain_string() const {
        if (!object) return field_name;
        if (auto* v = dynamic_cast<const VarNameNode*>(object.get()))
            return v->name + "." + field_name;
        if (auto* f = dynamic_cast<const FieldAccessNode*>(object.get())) {
            std::string sub = f->chain_string();
            if (!sub.empty()) return sub + "." + field_name;
        }
        return "";
    }

    void dump(int d) const override {
        indent(d);
        std::string chain = chain_string();
        if (!chain.empty()) {
            std::cout << "Field " << chain << "\n";
        } else {
            std::cout << "Field ." << field_name << "\n";
            if (object) object->dump(d + 1);
        }
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_field_access(*this); }
};

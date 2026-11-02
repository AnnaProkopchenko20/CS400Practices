#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "stmt_node.h"
#include "block_node.h"
#include "fn_param_node.h"
#include "lambda_capture.h"
#include "../visitor.h"

struct StructDeclNode;
struct DeclNode;

// Lambda statement: fn name := [captures](params) -> type NL block
// The parser only builds the surface form; the semantic checker lowers it to:
//   - a generated struct named "lambda.<name>" (lowered_struct)
//   - a variable declaration of that struct type (lowered_decl)
struct LambdaNode : StmtNode {
    std::string name;
    std::vector<std::unique_ptr<LambdaCapture>> captures;
    std::string return_type;
    std::vector<std::unique_ptr<FuncParamNode>> params;  // moved out during lowering
    std::unique_ptr<BlockNode>                  body;    // moved out during lowering

    // Filled by the semantic checker (Option A from the plan)
    StructDeclNode*       lowered_struct = nullptr; // non-owning (owned by generated_structs)
    std::unique_ptr<DeclNode> lowered_decl;

    LambdaNode(int l, int c,
               std::string n,
               std::vector<std::unique_ptr<LambdaCapture>> cap,
               std::string ret,
               std::vector<std::unique_ptr<FuncParamNode>> par,
               std::unique_ptr<BlockNode> b)
        : StmtNode(l, c)
        , name(std::move(n))
        , captures(std::move(cap))
        , return_type(std::move(ret))
        , params(std::move(par))
        , body(std::move(b)) {}

    void dump(int d) const override {
        indent(d);
        std::cout << "Lambda " << name << " -> " << return_type << "\n";
        if (!lowered_struct) {
            // parse-tree view (--ast): show raw captures, params, body
            for (const auto& cap : captures) cap->dump(d + 1);
            for (const auto& p   : params)   p->dump(d + 1);
            if (body) body->dump(d + 1);
        } else {
            // post-check view (--ast-checked): show lowered representation
            lowered_struct->dump(d + 1);
            if (lowered_decl) lowered_decl->dump(d + 1);
        }
    }

    llvm::Value* accept(Visitor& v) override { return v.visit_lambda(*this); }
};

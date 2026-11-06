#pragma once
#include <map>

#include "optimizer/walk_visitor.h"

// Counts the calls of every function. Only calls whose fn_decl is a GLOBAL
// function matter for the remover: a sibling member-function call has a
// MemberFnDeclNode in fn_decl, which is never a candidate. read() and print
// are separate node types, so they are never counted. Recursive calls count.
class FunctionUseCounter : public WalkVisitor
{
public:
    std::map<const FunctionDeclNode*, int> calls;

    llvm::Value* visit_fn_call(FunctionCallNode& n) override
    {
        if (n.fn_decl) calls[n.fn_decl] += 1;
        return WalkVisitor::visit_fn_call(n);
    }
};

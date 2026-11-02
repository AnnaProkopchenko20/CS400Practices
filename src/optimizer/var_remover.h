#pragma once
#include <memory>
#include <set>
#include <vector>

#include "optimizer/expr_util.h"
#include "optimizer/walk_visitor.h"

// Removes the dead declarations and every assignment to them.
// A statement list is REBUILT, never erased while iterating.
// An `if` / `while` whose block becomes empty stays: its condition is still evaluated.
class UnusedVariableRemover : public WalkVisitor
{
public:
    explicit UnusedVariableRemover(const std::set<const DeclNode*>& d) : dead(d) {}

    int removed = 0;

    llvm::Value* visit_block(BlockNode& n) override
    {
        filter(n.stmts);
        return WalkVisitor::visit_block(n);
    }

    llvm::Value* visit_program(ProgramNode& n) override
    {
        filter(n.stmts);
        return WalkVisitor::visit_program(n);
    }

private:
    const std::set<const DeclNode*>& dead;

    bool is_dead(StmtNode* s) const
    {
        if (auto* d = dynamic_cast<DeclNode*>(s))
            return !d->is_closure_init && dead.count(d) > 0;
        if (auto* a = dynamic_cast<AsmtNode*>(s))
        {
            DeclNode* r = root_decl(a->lhs.get());
            return r && dead.count(r) > 0;
        }
        return false;
    }

    void filter(std::vector<std::unique_ptr<StmtNode>>& stmts)
    {
        std::vector<std::unique_ptr<StmtNode>> keep;
        keep.reserve(stmts.size());
        for (auto& s : stmts)
        {
            if (s && is_dead(s.get())) { ++removed; continue; }   // subtree freed here
            keep.push_back(std::move(s));
        }
        stmts = std::move(keep);
    }
};

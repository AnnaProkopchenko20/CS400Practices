#pragma once
#include <map>
#include <set>

#include "optimizer/effects.h"
#include "optimizer/expr_util.h"
#include "optimizer/walk_visitor.h"

// Counts, per DECLARATION (not per name), the reads that resolve to it.
// The checker already stored the resolved declaration in VarNameNode::decl.
// Create a fresh counter every round: the maps are keyed by pointers.
class VariableUseCounter : public WalkVisitor
{
public:
    explicit VariableUseCounter(const EffectOracle& o) : oracle(o) {}

    llvm::Value* visit_decl(DeclNode& n) override
    {
        // Closure declarations (lambda lowering) are never removed.
        if (!n.is_closure_init)
        {
            candidates.insert(&n);
            reads.try_emplace(&n, 0);
        }
        if (n.expr)
        {
            // An initialiser that may be observed keeps its declaration.
            for (auto& item : n.expr->items)
            {
                if (item && oracle.has_effect(*item)) { reads[&n] += 1; break; }
            }
            n.expr->accept(*this);
        }
        return nullptr;
    }

    llvm::Value* visit_var(VarNameNode& n) override
    {
        if (n.decl) reads[n.decl] += 1;
        return nullptr;
    }

    // The target of an assignment is not a read. Exception: a value that may
    // be observed keeps the declaration of its target.
    llvm::Value* visit_asmt(AsmtNode& n) override
    {
        if (n.rhs && oracle.has_effect(*n.rhs))
        {
            if (DeclNode* d = root_decl(n.lhs.get())) reads[d] += 1;
        }
        if (n.rhs) n.rhs->accept(*this);
        return nullptr;
    }

    std::set<const DeclNode*> dead() const
    {
        std::set<const DeclNode*> out;
        for (const DeclNode* d : candidates)
        {
            auto it = reads.find(d);
            if (it == reads.end() || it->second == 0) out.insert(d);
        }
        return out;
    }

private:
    const EffectOracle& oracle;
    std::map<const DeclNode*, int> reads;
    std::set<const DeclNode*> candidates;
};

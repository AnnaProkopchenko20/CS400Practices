#include "optimizer/effects.h"

#include <map>
#include <vector>

#include "optimizer/walk_visitor.h"

namespace
{
    // One pass over the whole program: records, for every function / member
    // function / lambda `call`, whether it has a direct effect and whom it calls.
    struct GraphBuilder : WalkVisitor
    {
        FunctionDeclNode* current = nullptr;
        std::set<const FunctionDeclNode*> nodes;
        std::set<const FunctionDeclNode*> direct;
        std::map<const FunctionDeclNode*, std::set<const FunctionDeclNode*>> callees;

        void enter(FunctionDeclNode& n)
        {
            FunctionDeclNode* prev = current;
            current = &n;
            nodes.insert(&n);
            if (n.body) n.body->accept(*this);
            current = prev;
        }

        void mark_direct() { if (current) direct.insert(current); }

        void edge(const FunctionDeclNode* callee)
        {
            if (!current) return;                       // top-level code is not a node
            if (!callee) { direct.insert(current); return; }   // unresolved: unknown
            callees[current].insert(callee);
        }

        llvm::Value* visit_fn_decl(FunctionDeclNode& n) override
        {
            if (n.is_extern()) direct.insert(&n);
            enter(n);
            return nullptr;
        }

        llvm::Value* visit_member_fn_decl(MemberFnDeclNode& n) override
        {
            if (n.mutates) direct.insert(&n);
            enter(n);
            return nullptr;
        }

        llvm::Value* visit_while(WhileNode& n) override
        {
            mark_direct();
            return WalkVisitor::visit_while(n);
        }

        llvm::Value* visit_print(PrintNode& n) override
        {
            mark_direct();
            return WalkVisitor::visit_print(n);
        }

        llvm::Value* visit_read(ReadNode& n) override
        {
            mark_direct();
            return WalkVisitor::visit_read(n);
        }

        llvm::Value* visit_fn_call(FunctionCallNode& n) override
        {
            edge(n.fn_decl);   // may point at a MemberFnDeclNode (sibling call)
            return WalkVisitor::visit_fn_call(n);
        }

        llvm::Value* visit_member_call(MemberCallNode& n) override
        {
            edge(n.fn);
            return WalkVisitor::visit_member_call(n);
        }
    };

    // Looks inside ONE expression for something that may be observed.
    struct EffectFinder : WalkVisitor
    {
        const EffectOracle& oracle;
        bool found = false;

        explicit EffectFinder(const EffectOracle& o) : oracle(o) {}

        llvm::Value* visit_read(ReadNode&) override
        {
            found = true;
            return nullptr;
        }

        llvm::Value* visit_fn_call(FunctionCallNode& n) override
        {
            if (oracle.conservative || oracle.is_effectful(n.fn_decl)) found = true;
            return WalkVisitor::visit_fn_call(n);
        }

        llvm::Value* visit_member_call(MemberCallNode& n) override
        {
            if (oracle.conservative || oracle.is_effectful(n.fn)) found = true;
            return WalkVisitor::visit_member_call(n);
        }
    };
}

void EffectOracle::analyse(ProgramNode& program)
{
    effectful_.clear();
    if (conservative) return;

    GraphBuilder g;
    program.accept(g);

    // Rules 1-3a: direct effects.
    effectful_ = g.direct;

    // Rule: a function that can reach itself may never return.
    for (const FunctionDeclNode* f : g.nodes)
    {
        if (effectful_.count(f)) continue;

        std::set<const FunctionDeclNode*> seen;
        std::vector<const FunctionDeclNode*> stack;
        auto start = g.callees.find(f);
        if (start != g.callees.end())
            for (auto* c : start->second) stack.push_back(c);

        bool cycle = false;
        while (!stack.empty() && !cycle)
        {
            const FunctionDeclNode* c = stack.back();
            stack.pop_back();
            if (c == f) { cycle = true; break; }
            if (!seen.insert(c).second) continue;
            auto next = g.callees.find(c);
            if (next != g.callees.end())
                for (auto* d : next->second) stack.push_back(d);
        }
        if (cycle) effectful_.insert(f);
    }

    // Fixpoint: a caller of an effectful function is effectful.
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const auto& entry : g.callees)
        {
            const FunctionDeclNode* f = entry.first;
            if (effectful_.count(f)) continue;
            for (auto* c : entry.second)
            {
                if (effectful_.count(c))
                {
                    effectful_.insert(f);
                    changed = true;
                    break;
                }
            }
        }
    }
}

bool EffectOracle::has_effect(const ExprNode& expr) const
{
    EffectFinder finder(*this);
    const_cast<ExprNode&>(expr).accept(finder);
    return finder.found;
}

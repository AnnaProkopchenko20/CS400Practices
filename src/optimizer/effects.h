#pragma once
#include <set>
#include "ast.h"

// Decides whether evaluating an expression may be observed from outside.
//
//  conservative == true  : Task 1 rule. Any call or read() has an effect.
//  conservative == false : additional task. read(), or a call of a function in
//                          the effectful set, has an effect. The set is
//                          computed by analyse() and must be recomputed every
//                          round, because removals change the call graph.
//
// Effectful functions (global, member and lambda `call`):
//   * the body contains print or read(), or any `while`
//   * a member function that mutates its object (MemberFnDeclNode::mutates)
//   * a function that can reach itself in the call graph (may not return)
//   * a function that calls an effectful function (fixpoint)
//   * a function with an unresolved call (unknown -> keep)
class EffectOracle
{
public:
    bool conservative = false;

    void analyse(ProgramNode& program);
    bool has_effect(const ExprNode& expr) const;
    bool is_effectful(const FunctionDeclNode* fn) const
    {
        return !fn || effectful_.count(fn) > 0;
    }

private:
    std::set<const FunctionDeclNode*> effectful_;
};

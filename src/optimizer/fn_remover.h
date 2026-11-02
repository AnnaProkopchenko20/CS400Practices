#pragma once
#include <map>
#include <memory>
#include <vector>

#include "optimizer/walk_visitor.h"

// Removes global functions with zero calls, with their whole bodies.
// Member functions and lambdas are never removed.
class UnusedFunctionRemover : public WalkVisitor
{
public:
    explicit UnusedFunctionRemover(const std::map<const FunctionDeclNode*, int>& c) : calls(c) {}

    int removed = 0;

    llvm::Value* visit_program(ProgramNode& n) override
    {
        std::vector<std::unique_ptr<FunctionDeclNode>> keep;
        keep.reserve(n.functions.size());
        for (auto& f : n.functions)
        {
            auto it = calls.find(f.get());
            if (it != calls.end() && it->second > 0)
                keep.push_back(std::move(f));
            else
                ++removed;                                        // freed on reassignment below
        }
        n.functions = std::move(keep);
        return nullptr;
    }

private:
    const std::map<const FunctionDeclNode*, int>& calls;
};

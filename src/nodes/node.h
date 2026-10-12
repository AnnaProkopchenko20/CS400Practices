#pragma once
#include "../ast_fwd.h"
#include "../visitor.h"

struct Node {
    int line, col;
    Node(int l, int c) : line(l), col(c) {}
    virtual ~Node() = default;
    virtual void dump(int d) const = 0;
    virtual llvm::Value* accept(Visitor& v) = 0;
protected:
    static void indent(int d) { std::cout << string(d * 2, ' '); }
};

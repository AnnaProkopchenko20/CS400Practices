#pragma once
#include "../ast_fwd.h"
#include "../visitor.h"

struct Node {
    int line, col;

    // Number of Node objects alive right now. Used for the README memory
    // measurement: print it before and after the optimizer (--opt-stats).
    static inline int live = 0;

    Node(int l, int c) : line(l), col(c) { ++live; }
    virtual ~Node() { --live; }
    virtual void dump(int d) const = 0;
    virtual llvm::Value* accept(Visitor& v) = 0;
protected:
    static void indent(int d) { std::cout << string(d * 2, ' '); }
};

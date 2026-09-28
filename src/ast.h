#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <llvm/IR/Value.h>

using std::string; using std::vector; using std::unique_ptr; using std::make_unique;

struct ProgramNode; struct DeclNode; struct AsmtNode; struct ExitNode;
struct NumberNode; struct VarNameNode; struct BinaryOpNode;

struct Visitor {
    virtual ~Visitor() = default;
    virtual llvm::Value* visit_program(ProgramNode&) = 0;
    virtual llvm::Value* visit_decl(DeclNode&) = 0;
    virtual llvm::Value* visit_asmt(AsmtNode&) = 0;
    virtual llvm::Value* visit_exit(ExitNode&) = 0;
    virtual llvm::Value* visit_number(NumberNode&) = 0;
    virtual llvm::Value* visit_var(VarNameNode&) = 0;
    virtual llvm::Value* visit_binop(BinaryOpNode&) = 0;
};



struct Node {
    int line, col;
    Node(int l, int c) : line(l), col(c) {}
    virtual ~Node() = default;
    virtual void dump(int d) const = 0;
    virtual llvm::Value* accept(Visitor& v) = 0;
protected:
    static void indent(int d)
    {
        std::cout << string(d * 2, ' ');
    }
};

struct StmtNode : Node { using Node::Node; };
struct ExprNode : Node { using Node::Node; };
struct ValueNode : ExprNode { using ExprNode::ExprNode; };

struct NumberNode : ValueNode {
    int value;
    NumberNode(int l, int c, int v) : ValueNode(l, c), value(v) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Const " << value << "\n";
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_number(*this);
    }
};

struct VarNameNode : ValueNode {
    string name;
    VarNameNode(int l, int c, string n) : ValueNode(l, c), name(std::move(n)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Var " << name << "\n";
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_var(*this);
    }
};
struct BinaryOpNode : ExprNode {
    char op; unique_ptr<ExprNode> left, right;
    BinaryOpNode(int l, int c, char o, unique_ptr<ExprNode> lhs, unique_ptr<ExprNode> rhs)
        : ExprNode(l, c), op(o), left(std::move(lhs)), right(std::move(rhs)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "BinOp " << op << "\n";
        left->dump(d + 1); right->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_binop(*this);
    }
};
struct DeclNode : StmtNode {
    string name; bool is_mut; unique_ptr<ExprNode> expr;
    DeclNode(int l, int c, string n, bool m, unique_ptr<ExprNode> e)
        : StmtNode(l, c), name(std::move(n)), is_mut(m), expr(std::move(e)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Decl " << name << (is_mut ? " mut" : " const") << "\n";
        expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_decl(*this);
    }
};
struct AsmtNode : StmtNode {
    string name; unique_ptr<ExprNode> expr;
    AsmtNode(int l, int c, string n, unique_ptr<ExprNode> e)
        : StmtNode(l, c), name(std::move(n)), expr(std::move(e)) {}
    void dump(int d) const override
    {
        indent(d); std::cout << "Assign " << name << "\n";
        expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_asmt(*this);
    }
};
struct ExitNode : Node {
    unique_ptr<ExprNode> expr;
    ExitNode(int l, int c, unique_ptr<ExprNode> e) : Node(l, c), expr(std::move(e)) {}
    void dump(int d) const override
    {
        indent(d); std::cout << "Exit\n"; expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_exit(*this);
    }
};
struct ProgramNode : Node {
    vector<unique_ptr<StmtNode>> stmts; unique_ptr<ExitNode> exit_stmt;
    ProgramNode(int l, int c, vector<unique_ptr<StmtNode>> s, unique_ptr<ExitNode> e)
        :Node(l, c), stmts(std::move(s)), exit_stmt(std::move(e)) {}
    void dump(int d) const override {
        indent(d); std::cout << "Program\n";
        for (auto& s : stmts)
        {
            s->dump(d + 1);
        }
        exit_stmt->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_program(*this);
    }
};

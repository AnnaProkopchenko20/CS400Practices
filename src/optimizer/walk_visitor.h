#pragma once
// Default traversal: visits every child of every node and returns nullptr.
// Optimizer visitors derive from this and override only what they care about.
//
// Rules that keep the counts right:
//  * Lambda bodies live in program->structs (the generated "lambda.*" structs),
//    so they are walked there, exactly once. visit_lambda walks ONLY the closure
//    declaration (the capture initialisers), never the body.
//  * Parameters (FuncParamNode::decl) are never walked: they are not candidates.
#include "ast.h"
#include "visitor.h"

struct WalkVisitor : Visitor
{
    llvm::Value* visit_program(ProgramNode& n) override
    {
        for (auto& s : n.structs)   if (s) s->accept(*this);
        for (auto& f : n.functions) if (f) f->accept(*this);
        for (auto& s : n.stmts)     if (s) s->accept(*this);
        if (n.exit_stmt) n.exit_stmt->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_decl(DeclNode& n) override
    {
        if (n.expr) n.expr->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_asmt(AsmtNode& n) override
    {
        if (n.lhs) n.lhs->accept(*this);
        if (n.rhs) n.rhs->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_exit(ExitNode& n) override
    {
        if (n.expr) n.expr->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_number(NumberNode&) override { return nullptr; }
    llvm::Value* visit_bool(BoolNode&)     override { return nullptr; }
    llvm::Value* visit_var(VarNameNode&)   override { return nullptr; }
    llvm::Value* visit_this(ThisNode&)     override { return nullptr; }

    llvm::Value* visit_if(IfNode& n) override
    {
        if (n.cond)       n.cond->accept(*this);
        if (n.then_block) n.then_block->accept(*this);
        if (n.else_block) n.else_block->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_while(WhileNode& n) override
    {
        if (n.cond) n.cond->accept(*this);
        if (n.body) n.body->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_block(BlockNode& n) override
    {
        for (auto& s : n.stmts) if (s) s->accept(*this);
        if (n.exit_stmt) n.exit_stmt->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_not(NotNode& n) override
    {
        if (n.expr) n.expr->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_binop(BinaryOpNode& n) override
    {
        if (n.left)  n.left->accept(*this);
        if (n.right) n.right->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_struct_decl(StructDeclNode& n) override
    {
        for (auto& m : n.member_fns) if (m) m->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_fn_decl(FunctionDeclNode& n) override
    {
        if (n.body) n.body->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_fn_call(FunctionCallNode& n) override
    {
        for (auto& a : n.args) if (a) a->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_field_access(FieldAccessNode& n) override
    {
        if (n.object) n.object->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_init(InitNode& n) override
    {
        for (auto& i : n.items) if (i) i->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_member_fn_decl(MemberFnDeclNode& n) override
    {
        if (n.body) n.body->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_member_call(MemberCallNode& n) override
    {
        if (n.object) n.object->accept(*this);
        for (auto& a : n.args) if (a) a->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_print(PrintNode& n) override
    {
        if (!n.is_string && n.expr) n.expr->accept(*this);
        return nullptr;
    }

    llvm::Value* visit_read(ReadNode&) override { return nullptr; }

    // Only the closure declaration (capture initialisers). The body is walked
    // through program->structs.
    llvm::Value* visit_lambda(LambdaNode& n) override
    {
        if (n.lowered_decl) n.lowered_decl->accept(*this);
        return nullptr;
    }
};

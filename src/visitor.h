#pragma once
#include "ast_fwd.h"
#include <llvm/IR/Value.h>

// Pure-virtual interface implemented by every AST tree walk.
struct Visitor {
    virtual ~Visitor() = default;
    virtual llvm::Value* visit_program(ProgramNode&)  = 0;
    virtual llvm::Value* visit_decl(DeclNode&)        = 0;
    virtual llvm::Value* visit_asmt(AsmtNode&)        = 0;
    virtual llvm::Value* visit_exit(ExitNode&)        = 0;
    virtual llvm::Value* visit_number(NumberNode&)    = 0;
    virtual llvm::Value* visit_bool(BoolNode&)        = 0;
    virtual llvm::Value* visit_var(VarNameNode&)      = 0;
    virtual llvm::Value* visit_binop(BinaryOpNode&)   = 0;
    virtual llvm::Value* visit_if(IfNode&)            = 0;
    virtual llvm::Value* visit_block(BlockNode&)      = 0;
    virtual llvm::Value* visit_not(NotNode&)          = 0;

    //visitor functions for all new node types
};

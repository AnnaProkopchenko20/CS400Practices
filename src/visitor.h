#pragma once

#include "ast_fwd.h"

namespace llvm {
    class Value;
}

struct Visitor {
    virtual ~Visitor() = default;

    virtual llvm::Value* visit_program(ProgramNode&)  = 0;
    virtual llvm::Value* visit_decl(DeclNode&)        = 0;
    virtual llvm::Value* visit_asmt(AsmtNode&)        = 0;
    virtual llvm::Value* visit_exit(ExitNode&)        = 0;
    virtual llvm::Value* visit_number(NumberNode&)    = 0;
    virtual llvm::Value* visit_bool(BoolNode&)        = 0;
    virtual llvm::Value* visit_var(VarNameNode&)      = 0;
    virtual llvm::Value* visit_if(IfNode&)            = 0;
    virtual llvm::Value* visit_while(WhileNode&)      = 0;
    virtual llvm::Value* visit_block(BlockNode&)      = 0;
    virtual llvm::Value* visit_not(NotNode&)          = 0;
    virtual llvm::Value* visit_binop(BinaryOpNode&)   = 0;

    virtual llvm::Value* visit_struct_decl(StructDeclNode&)         = 0;
    virtual llvm::Value* visit_fn_decl(FunctionDeclNode&)           = 0;
    virtual llvm::Value* visit_fn_call(FunctionCallNode&)           = 0;
    virtual llvm::Value* visit_field_access(StructFieldAccessNode&) = 0;
    virtual llvm::Value* visit_init(InitNode&)                      = 0;
};

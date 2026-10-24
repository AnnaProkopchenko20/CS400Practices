#pragma once
#include <map>
#include <memory>
#include <string>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "ast.h"
#include "semantic_checker.h"
#include "visitor.h"

struct CodeGen : Visitor
{
    llvm::LLVMContext             context;
    llvm::IRBuilder<>             builder;
    std::unique_ptr<llvm::Module> module;

    llvm::Type* i32_type = nullptr;
    llvm::Type* i64_type = nullptr;
    llvm::Type* i1_type  = nullptr;

    llvm::Function*      main_func   = nullptr;
    llvm::FunctionCallee printf_func;
    llvm::Value*         fmt_int     = nullptr;
    llvm::Value*         fmt_bool    = nullptr;
    llvm::Value*         true_str    = nullptr;
    llvm::Value*         false_str   = nullptr;

    std::map<DeclNode*, llvm::AllocaInst*>    allocas;
    std::map<std::string, llvm::StructType*>  struct_types;
    std::map<std::string, llvm::Function*>    ir_functions;
    std::map<std::string, StructInfo>         structs;
    FunctionDeclNode*                         current_fn = nullptr;

    CodeGen();

    llvm::Value* visit_program(ProgramNode& n) override;
    llvm::Value* visit_decl(DeclNode& n)       override;
    llvm::Value* visit_asmt(AsmtNode& n)       override;
    llvm::Value* visit_exit(ExitNode& n)       override;
    llvm::Value* visit_number(NumberNode& n)   override;
    llvm::Value* visit_bool(BoolNode& n)       override;
    llvm::Value* visit_var(VarNameNode& n)     override;
    llvm::Value* visit_if(IfNode& n)           override;
    llvm::Value* visit_while(WhileNode& n)     override;
    llvm::Value* visit_block(BlockNode& n)     override;
    llvm::Value* visit_not(NotNode& n)         override;
    llvm::Value* visit_binop(BinaryOpNode& n)  override;
    llvm::Value* visit_struct_decl(StructDeclNode& n)         override;
    llvm::Value* visit_fn_decl(FunctionDeclNode& n)           override;
    llvm::Value* visit_fn_call(FunctionCallNode& n)           override;
    llvm::Value* visit_field_access(StructFieldAccessNode& n) override;
    llvm::Value* visit_init(InitNode& n)                      override;

private:
    llvm::Type*       llvm_type(const std::string& name);
    llvm::Value*      coerce(llvm::Value* v, const std::string& have, const std::string& want);
    llvm::AllocaInst* create_entry_alloca(llvm::Type* type, const std::string& name);
    bool              current_block_terminated();
    void              emit_exit(ExitNode& n);
};

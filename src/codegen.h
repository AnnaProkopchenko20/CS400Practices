#pragma once
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/TargetParser/Host.h>
#include "ast.h"

struct VariableInfo
{
    llvm::AllocaInst* alloca;
    bool is_mut;
};

struct CodeGen : Visitor {
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    std::map<std::string, VariableInfo> symbols;
    llvm::Type* i32_type;
    llvm::FunctionCallee printf_func;
    llvm::Constant* fmt_str;

    CodeGen() : builder(context) {
        module = std::make_unique<llvm::Module>("practice2", context);
        module->setTargetTriple(llvm::sys::getDefaultTargetTriple());

        i32_type = llvm::Type::getInt32Ty(context);
    }

    [[noreturn]] void fail(const Node& n, const std::string& msg) {
        throw std::runtime_error("line " + std::to_string(n.line) + ":" + std::to_string(n.col) + ": " + msg);
    }

    llvm::Value* visit_program(ProgramNode& n) override {
        llvm::FunctionType *main_func_type = llvm::FunctionType::get(i32_type, false);
        llvm::Function *main_func = llvm::Function::Create(main_func_type, llvm::Function::ExternalLinkage, "main", module.get());
        llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(context, "entry", main_func);
        builder.SetInsertPoint(entry_bb);
        fmt_str = builder.CreateGlobalStringPtr("Program exit with result %d\n", "fmt");

        llvm::FunctionType *printf_type = llvm::FunctionType::get(i32_type, {llvm::PointerType::getUnqual(context)}, true /* var_arg */);
        printf_func = module->getOrInsertFunction("printf", printf_type);

        for (auto& s : n.stmts)
        {
            s->accept(*this);
        }
        n.exit_stmt->accept(*this);

        return nullptr;
    }

    llvm::Value* visit_decl(DeclNode& n) override {
        if (symbols.count(n.name)) {
            fail(n, "variable '" + n.name + "' is already defined");
        }

        llvm::Value* val = n.expr->accept(*this);

        llvm::AllocaInst* alloca = builder.CreateAlloca(i32_type, nullptr, n.name);

        builder.CreateStore(val, alloca);

        symbols[n.name] = VariableInfo{alloca, n.is_mut};

        return nullptr;
    }

    llvm::Value* visit_asmt(AsmtNode& n) override {
        if (!symbols.count(n.name)) {
            fail(n, "unknown variable '" + n.name + "'");
        }
        if (!symbols[n.name].is_mut) {
            fail(n, "cannot assign to a const variable '" + n.name + "'");
        }

        llvm::Value* val = n.expr->accept(*this);

        builder.CreateStore(val, symbols[n.name].alloca);

        return nullptr;
    }

    llvm::Value* visit_exit(ExitNode& n) override {

        llvm::Value* val = n.expr->accept(*this);

        builder.CreateCall(printf_func, {fmt_str, val});
        builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));

        return nullptr;
    }

    llvm::Value* visit_number(NumberNode& n) override {
        return llvm::ConstantInt::get(i32_type, n.value);
    }

    llvm::Value* visit_var(VarNameNode& n) override {
        if (!symbols.count(n.name)) {
            fail(n, "unknown variable '" + n.name + "'");
        }

        return builder.CreateLoad(i32_type, symbols[n.name].alloca, n.name);
    }

    llvm::Value* visit_binop(BinaryOpNode& n) override {
        llvm::Value* l = n.left->accept(*this);
        llvm::Value* r = n.right->accept(*this);

        switch (n.op) {
            case '+': return builder.CreateAdd(l, r, "addtmp");
            case '-': return builder.CreateSub(l, r, "subtmp");
            case '*': return builder.CreateMul(l, r, "multmp");
            default:  fail(n, std::string("unknown binary operator '") + n.op + "'");
        }
    }
};

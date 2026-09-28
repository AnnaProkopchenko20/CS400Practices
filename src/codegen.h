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

// Generates IR from a tree the SemanticChecker has already accepted.
// It performs no checks and no lookups by name: every expression carries its
// type (node.type) and every variable/assignment carries its declaration (node.decl).
struct CodeGen : Visitor {
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    std::map<DeclNode*, llvm::AllocaInst*> allocas;   // declaration -> its stack slot
    llvm::Type* i1_type;
    llvm::Type* i32_type;
    llvm::Type* i64_type;
    llvm::FunctionCallee printf_func;
    llvm::Constant* fmt_int;    // "Program exit with result %lld\n"
    llvm::Constant* fmt_bool;   // "Program exit with result %s\n"
    llvm::Constant* true_str;
    llvm::Constant* false_str;

    CodeGen() : builder(context) {
        module = std::make_unique<llvm::Module>("practice4", context);
        module->setTargetTriple(llvm::sys::getDefaultTargetTriple());

        i1_type = llvm::Type::getInt1Ty(context);
        i32_type = llvm::Type::getInt32Ty(context);
        i64_type = llvm::Type::getInt64Ty(context);
    }

    // language type name -> LLVM type
    llvm::Type* llvm_type(const std::string& name) {
        if (name == "i32") return i32_type;
        if (name == "i64") return i64_type;
        return i1_type;   // "bool"
    }

    // The one implicit conversion of the language: i32 -> i64.
    llvm::Value* coerce(llvm::Value* value, const std::string& have, const std::string& want) {
        if (have == "i32" && want == "i64")
            return builder.CreateSExt(value, i64_type, "wide");
        return value;
    }

    llvm::Value* visit_program(ProgramNode& n) override {
        llvm::FunctionType* main_func_type = llvm::FunctionType::get(i32_type, false);
        llvm::Function* main_func = llvm::Function::Create(main_func_type, llvm::Function::ExternalLinkage, "main", module.get());
        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(context, "entry", main_func);
        builder.SetInsertPoint(entry_bb);

        fmt_int = builder.CreateGlobalStringPtr("Program exit with result %lld\n", "fmt_int");
        fmt_bool = builder.CreateGlobalStringPtr("Program exit with result %s\n", "fmt_bool");
        true_str = builder.CreateGlobalStringPtr("true", "str_true");
        false_str = builder.CreateGlobalStringPtr("false", "str_false");

        llvm::FunctionType* printf_type = llvm::FunctionType::get(i32_type, {llvm::PointerType::getUnqual(context)}, true /* var_arg */);
        printf_func = module->getOrInsertFunction("printf", printf_type);

        for (auto& s : n.stmts)
        {
            s->accept(*this);
        }
        n.exit_stmt->accept(*this);

        return nullptr;
    }

    llvm::Value* visit_decl(DeclNode& n) override {
        llvm::Value* val = n.expr->accept(*this);
        val = coerce(val, n.expr->type, n.type_name);

        llvm::AllocaInst* slot = builder.CreateAlloca(llvm_type(n.type_name), nullptr, n.name);
        builder.CreateStore(val, slot);

        allocas[&n] = slot;

        return nullptr;
    }

    llvm::Value* visit_asmt(AsmtNode& n) override {
        llvm::Value* val = n.expr->accept(*this);
        val = coerce(val, n.expr->type, n.decl->type_name);

        builder.CreateStore(val, allocas[n.decl]);

        return nullptr;
    }

    llvm::Value* visit_exit(ExitNode& n) override {
        llvm::Value* val = n.expr->accept(*this);

        if (n.expr->type == "bool")
        {
            // no branches yet: pick the string with a select
            llvm::Value* text = builder.CreateSelect(val, true_str, false_str, "booltext");
            builder.CreateCall(printf_func, {fmt_bool, text});
        }
        else
        {
            val = coerce(val, n.expr->type, "i64");
            builder.CreateCall(printf_func, {fmt_int, val});
        }

        builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));

        return nullptr;
    }

    llvm::Value* visit_number(NumberNode& n) override {
        return llvm::ConstantInt::get(llvm_type(n.type), n.value, true);
    }

    llvm::Value* visit_bool(BoolNode& n) override {
        return llvm::ConstantInt::get(i1_type, n.value ? 1 : 0);
    }

    llvm::Value* visit_var(VarNameNode& n) override {
        return builder.CreateLoad(llvm_type(n.type), allocas[n.decl], n.name);
    }

    llvm::Value* visit_binop(BinaryOpNode& n) override {
        llvm::Value* l = n.left->accept(*this);
        llvm::Value* r = n.right->accept(*this);

        if (n.op == "+" || n.op == "-" || n.op == "*")
        {
            // both operands are widened to the type of the result
            l = coerce(l, n.left->type, n.type);
            r = coerce(r, n.right->type, n.type);

            if (n.op == "+") return builder.CreateAdd(l, r, "addtmp");
            if (n.op == "-") return builder.CreateSub(l, r, "subtmp");
            return builder.CreateMul(l, r, "multmp");
        }

        // == and !=: both operands to the same width, then icmp -> i1
        std::string common = (n.left->type == "i64" || n.right->type == "i64") ? "i64" : n.left->type;
        l = coerce(l, n.left->type, common);
        r = coerce(r, n.right->type, common);

        if (n.op == "==") return builder.CreateICmpEQ(l, r, "eqtmp");
        return builder.CreateICmpNE(l, r, "netmp");
    }
};

#include "codegen.h"
#include <llvm/TargetParser/Host.h>

CodeGen::CodeGen() : builder(context)
{
    module = std::make_unique<llvm::Module>("practice5", context);
    module->setTargetTriple(llvm::sys::getDefaultTargetTriple());
    i1_type  = llvm::Type::getInt1Ty(context);
    i32_type = llvm::Type::getInt32Ty(context);
    i64_type = llvm::Type::getInt64Ty(context);
}

llvm::Type* CodeGen::llvm_type(const std::string& name)
{
    if (name == "i32") return i32_type;
    if (name == "i64") return i64_type;
    return i1_type;
}

llvm::Value* CodeGen::coerce(llvm::Value* value,
                              const std::string& have, const std::string& want)
{
    if (have == "i32" && want == "i64")
        return builder.CreateSExt(value, i64_type, "wide");
    return value;
}

llvm::AllocaInst* CodeGen::create_entry_alloca(llvm::Type* type, const std::string& name)
{
    llvm::BasicBlock& entry = main_func->getEntryBlock();
    llvm::IRBuilder<> tmp(&entry, entry.begin());
    return tmp.CreateAlloca(type, nullptr, name);
}

bool CodeGen::current_block_terminated()
{
    return builder.GetInsertBlock()->getTerminator() != nullptr;
}

void CodeGen::emit_exit(ExitNode& n)
{
    llvm::Value* val = n.expr->accept(*this);
    if (n.expr->type == "bool")
    {
        llvm::Value* text = builder.CreateSelect(val, true_str, false_str, "booltext");
        builder.CreateCall(printf_func, {fmt_bool, text});
    }
    else
    {
        val = coerce(val, n.expr->type, "i64");
        builder.CreateCall(printf_func, {fmt_int, val});
    }
    builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
}

llvm::Value* CodeGen::visit_program(ProgramNode& n)
{
    llvm::FunctionType* ft = llvm::FunctionType::get(i32_type, false);
    main_func = llvm::Function::Create(
        ft, llvm::Function::ExternalLinkage, "main", module.get());
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "entry", main_func);
    builder.SetInsertPoint(bb);

    fmt_int   = builder.CreateGlobalStringPtr("Program exit with result %lld\n", "fmt_int");
    fmt_bool  = builder.CreateGlobalStringPtr("Program exit with result %s\n",   "fmt_bool");
    true_str  = builder.CreateGlobalStringPtr("true",  "str_true");
    false_str = builder.CreateGlobalStringPtr("false", "str_false");

    llvm::FunctionType* pft = llvm::FunctionType::get(
        i32_type, {llvm::PointerType::getUnqual(context)}, true);
    printf_func = module->getOrInsertFunction("printf", pft);

    for (auto& s : n.stmts) s->accept(*this);
    n.exit_stmt->accept(*this);
    return nullptr;
}

llvm::Value* CodeGen::visit_block(BlockNode& n)
{
    for (auto& s : n.stmts) s->accept(*this);
    if (n.exit_stmt) n.exit_stmt->accept(*this);
    return nullptr;
}

llvm::Value* CodeGen::visit_decl(DeclNode& n)
{
    llvm::Value* val = n.expr->accept(*this);
    val = coerce(val, n.expr->type, n.type_name);
    llvm::AllocaInst* slot = create_entry_alloca(llvm_type(n.type_name), n.name);
    builder.CreateStore(val, slot);
    allocas[&n] = slot;
    return nullptr;
}

llvm::Value* CodeGen::visit_asmt(AsmtNode& n)
{
    llvm::Value* val = n.expr->accept(*this);
    val = coerce(val, n.expr->type, n.decl->type_name);
    builder.CreateStore(val, allocas[n.decl]);
    return nullptr;
}

llvm::Value* CodeGen::visit_exit(ExitNode& n) { emit_exit(n); return nullptr; }

llvm::Value* CodeGen::visit_number(NumberNode& n)
{
    return llvm::ConstantInt::get(llvm_type(n.type), n.value, true);
}

llvm::Value* CodeGen::visit_bool(BoolNode& n)
{
    return llvm::ConstantInt::get(i1_type, n.value ? 1 : 0);
}

llvm::Value* CodeGen::visit_var(VarNameNode& n)
{
    return builder.CreateLoad(llvm_type(n.type), allocas[n.decl], n.name);
}

llvm::Value* CodeGen::visit_binop(BinaryOpNode& n)
{
    llvm::Value* l = n.left->accept(*this);
    llvm::Value* r = n.right->accept(*this);
    if (n.op == "+" || n.op == "-" || n.op == "*")
    {
        l = coerce(l, n.left->type, n.type);
        r = coerce(r, n.right->type, n.type);
        if (n.op == "+") return builder.CreateAdd(l, r, "addtmp");
        if (n.op == "-") return builder.CreateSub(l, r, "subtmp");
        return builder.CreateMul(l, r, "multmp");
    }
    std::string common = (n.left->type == "i64" || n.right->type == "i64") ? "i64" : n.left->type;
    l = coerce(l, n.left->type,  common);
    r = coerce(r, n.right->type, common);
    if (n.op == "==") return builder.CreateICmpEQ(l, r, "eqtmp");
    return builder.CreateICmpNE(l, r, "netmp");
}

llvm::Value* CodeGen::visit_if(IfNode& n)
{
    llvm::Value* cond = n.cond->accept(*this);
    auto* then_bb  = llvm::BasicBlock::Create(context, "then",  main_func);
    auto* else_bb  = n.else_block ? llvm::BasicBlock::Create(context, "else", main_func) : nullptr;
    auto* merge_bb = llvm::BasicBlock::Create(context, "merge", main_func);
    builder.CreateCondBr(cond, then_bb, else_bb ? else_bb : merge_bb);

    builder.SetInsertPoint(then_bb);
    n.then_block->accept(*this);
    if (!current_block_terminated()) builder.CreateBr(merge_bb);

    if (else_bb)
    {
        builder.SetInsertPoint(else_bb);
        n.else_block->accept(*this);
        if (!current_block_terminated()) builder.CreateBr(merge_bb);
    }
    builder.SetInsertPoint(merge_bb);
    return nullptr;
}

llvm::Value* CodeGen::visit_while(WhileNode& n)
{
    auto* cond_bb = llvm::BasicBlock::Create(context, "while.cond", main_func);
    auto* body_bb = llvm::BasicBlock::Create(context, "while.body", main_func);
    auto* end_bb  = llvm::BasicBlock::Create(context, "while.end",  main_func);
    builder.CreateBr(cond_bb);

    builder.SetInsertPoint(cond_bb);
    llvm::Value* cond = n.cond->accept(*this);
    builder.CreateCondBr(cond, body_bb, end_bb);

    builder.SetInsertPoint(body_bb);
    n.body->accept(*this);
    if (!current_block_terminated()) builder.CreateBr(cond_bb);

    builder.SetInsertPoint(end_bb);
    return nullptr;
}

llvm::Value* CodeGen::visit_not(NotNode& n)
{
    return builder.CreateNot(n.expr->accept(*this), "nottmp");
}

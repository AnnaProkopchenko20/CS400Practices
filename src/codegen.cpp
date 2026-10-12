#include "codegen.h"

CodeGen::CodeGen() : builder(context)
{
    module   = std::make_unique<llvm::Module>("main", context);
    i1_type  = llvm::Type::getInt1Ty(context);
    i32_type = llvm::Type::getInt32Ty(context);
    i64_type = llvm::Type::getInt64Ty(context);
}

llvm::Type* CodeGen::llvm_type(const std::string& name)
{
    if (name == "i32") return i32_type;
    if (name == "i64") return i64_type;
    if (name == "bool") return i1_type;
    auto it = struct_types.find(name);
    if (it != struct_types.end()) return it->second;
    return nullptr;
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
    llvm::Function* f = current_fn ? ir_functions[current_fn->name] : main_func;
    llvm::BasicBlock& entry = f->getEntryBlock();
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
    if (current_fn)
    {
        val = coerce(val, n.expr->type, current_fn->return_type);
        builder.CreateRet(val);
    }
    else
    {
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
}

llvm::Value* CodeGen::visit_program(ProgramNode& n)
{
    for (auto& s : n.structs)
    {
        llvm::StructType* st = llvm::StructType::create(context, s->name);
        struct_types[s->name] = st;
        StructInfo sinfo;
        sinfo.name = s->name;
        for (size_t i = 0; i < s->fields.size(); ++i)
        {
            sinfo.field_index[s->fields[i]->name] = (int)i;
            sinfo.fields.push_back({s->fields[i]->type_name, s->fields[i]->name, s->fields[i]->is_mut, (int)i});
        }
        structs[s->name] = std::move(sinfo);
    }

    for (auto& s : n.structs)
    {
        std::vector<llvm::Type*> member_types;
        for (const auto& f : s->fields)
            member_types.push_back(llvm_type(f->type_name));
        struct_types[s->name]->setBody(member_types);
    }

    for (auto& fn : n.functions)
    {
        std::vector<llvm::Type*> param_types;
        for (const auto& p : fn->params)
            param_types.push_back(llvm_type(p->type_name));
        llvm::Type* ret_t = llvm_type(fn->return_type);
        llvm::FunctionType* ft = llvm::FunctionType::get(ret_t, param_types, false);
        llvm::Function* f = llvm::Function::Create(
            ft, llvm::Function::ExternalLinkage, "fn." + fn->name, module.get());
        ir_functions[fn->name] = f;
    }

    for (auto& fn : n.functions)
        fn->accept(*this);

    current_fn = nullptr;
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

llvm::Value* CodeGen::visit_struct_decl(StructDeclNode& n) { return nullptr; }

llvm::Value* CodeGen::visit_fn_decl(FunctionDeclNode& n)
{
    current_fn = &n;
    llvm::Function* f = ir_functions[n.name];
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "entry", f);
    builder.SetInsertPoint(bb);

    auto arg_it = f->arg_begin();
    for (size_t i = 0; i < n.params.size(); ++i, ++arg_it)
    {
        const auto& p = n.params[i];
        arg_it->setName(p->name);
        llvm::AllocaInst* slot = create_entry_alloca(llvm_type(p->type_name), p->name + ".1");
        builder.CreateStore(&*arg_it, slot);
        allocas[p->decl.get()] = slot;
    }

    n.body->accept(*this);
    current_fn = nullptr;
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
    llvm::AllocaInst* slot = create_entry_alloca(llvm_type(n.type_name), n.name);
    allocas[&n] = slot;

    if (struct_types.count(n.type_name))
    {
        if (n.expr->items.size() == 1 && n.expr->items[0]->type == n.type_name)
        {
            llvm::Value* val = n.expr->items[0]->accept(*this);
            builder.CreateStore(val, slot);
        }
        else
        {
            const auto& sinfo = structs[n.type_name];
            for (size_t i = 0; i < n.expr->items.size(); ++i)
            {
                llvm::Value* field_ptr = builder.CreateStructGEP(
                    llvm_type(n.type_name), slot, (unsigned)i,
                    n.name + "." + std::to_string(i) + ".ptr");
                llvm::Value* fval = n.expr->items[i]->accept(*this);
                fval = coerce(fval, n.expr->items[i]->type, sinfo.fields[i].type);
                builder.CreateStore(fval, field_ptr);
            }
        }
    }
    else
    {
        llvm::Value* val = n.expr->items[0]->accept(*this);
        val = coerce(val, n.expr->items[0]->type, n.type_name);
        builder.CreateStore(val, slot);
    }
    return nullptr;
}

llvm::Value* CodeGen::visit_asmt(AsmtNode& n)
{
    llvm::Value* val = n.expr->accept(*this);
    if (n.field_names.empty())
    {
        val = coerce(val, n.expr->type, n.decl->type_name);
        builder.CreateStore(val, allocas[n.decl]);
    }
    else
    {
        llvm::Value* ptr = allocas[n.decl];
        std::string cur_type = n.decl->type_name;
        for (int index : n.path)
        {
            ptr = builder.CreateStructGEP(
                llvm_type(cur_type), ptr, (unsigned)index, n.name + ".ptr");
            cur_type = structs[cur_type].fields[index].type;
        }
        val = coerce(val, n.expr->type, cur_type);
        builder.CreateStore(val, ptr);
    }
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
    llvm::Function* cur_f = current_fn ? ir_functions[current_fn->name] : main_func;
    llvm::Value* cond = n.cond->accept(*this);
    auto* then_bb  = llvm::BasicBlock::Create(context, "then",  cur_f);
    auto* else_bb  = n.else_block ? llvm::BasicBlock::Create(context, "else", cur_f) : nullptr;
    auto* merge_bb = llvm::BasicBlock::Create(context, "merge", cur_f);
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
    llvm::Function* cur_f = current_fn ? ir_functions[current_fn->name] : main_func;
    auto* cond_bb = llvm::BasicBlock::Create(context, "while.cond", cur_f);
    auto* body_bb = llvm::BasicBlock::Create(context, "while.body", cur_f);
    auto* end_bb  = llvm::BasicBlock::Create(context, "while.end",  cur_f);
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

llvm::Value* CodeGen::visit_fn_call(FunctionCallNode& n)
{
    llvm::Function* f = ir_functions[n.name];
    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < n.args.size(); ++i)
    {
        llvm::Value* arg_val = n.args[i]->accept(*this);
        arg_val = coerce(arg_val, n.args[i]->type, n.fn_decl->params[i]->type_name);
        args.push_back(arg_val);
    }
    return builder.CreateCall(f, args, n.name + ".ret");
}

llvm::Value* CodeGen::visit_field_access(StructFieldAccessNode& n)
{
    llvm::Value* ptr = allocas[n.base_decl];
    std::string cur_type = n.base_decl->type_name;
    for (int index : n.path)
    {
        ptr = builder.CreateStructGEP(
            llvm_type(cur_type), ptr, (unsigned)index, n.base_name + ".ptr");
        cur_type = structs[cur_type].fields[index].type;
    }
    return builder.CreateLoad(llvm_type(n.type), ptr, n.base_name + ".val");
}

llvm::Value* CodeGen::visit_init(InitNode& n)
{
    llvm::Type* st = llvm_type(n.type);
    llvm::Value* agg = llvm::UndefValue::get(st);
    const auto& sinfo = structs[n.type];
    for (size_t i = 0; i < n.items.size(); ++i)
    {
        llvm::Value* fval = n.items[i]->accept(*this);
        fval = coerce(fval, n.items[i]->type, sinfo.fields[i].type);
        agg = builder.CreateInsertValue(agg, fval, {(unsigned)i}, "agg");
    }
    return agg;
}

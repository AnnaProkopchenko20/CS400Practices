#include "codegen.h"

#include <iostream>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

CodeGen::CodeGen() : builder(context)
{
    module   = std::make_unique<llvm::Module>("main", context);
    i32_type = llvm::Type::getInt32Ty(context);
    i64_type = llvm::Type::getInt64Ty(context);
    i1_type  = llvm::Type::getInt1Ty(context);
}

std::string CodeGen::member_fn_ir_name(const std::string& struct_name, const std::string& fn_name)
{
    if (struct_name.find('_') != std::string::npos || fn_name.find('_') != std::string::npos)
    {
        std::string s_esc;
        for (char ch : struct_name) {
            if (ch == '_') s_esc += "__";
            else s_esc += ch;
        }
        return s_esc + "_" + fn_name;
    }
    return struct_name + "_" + fn_name;
}

llvm::Type* CodeGen::llvm_type(const std::string& name)
{
    if (name == "i32")  return i32_type;
    if (name == "i64")  return i64_type;
    if (name == "bool") return i1_type;
    auto it = struct_types.find(name);
    if (it != struct_types.end()) return it->second;
    throw std::runtime_error("unknown type in codegen: " + name);
}

llvm::Value* CodeGen::coerce(llvm::Value* v, const std::string& have, const std::string& want)
{
    if (have == want) return v;
    if (have == "i32" && want == "i64")
        return builder.CreateSExt(v, i64_type, "sext_tmp");
    return v;
}

llvm::AllocaInst* CodeGen::create_entry_alloca(llvm::Type* type, const std::string& name)
{
    llvm::Function* fn = builder.GetInsertBlock()->getParent();
    llvm::IRBuilder<> entry_builder(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    return entry_builder.CreateAlloca(type, nullptr, name);
}

bool CodeGen::current_block_terminated()
{
    llvm::BasicBlock* bb = builder.GetInsertBlock();
    return bb && bb->getTerminator() != nullptr;
}

llvm::Value* CodeGen::get_expr_address(ExprNode& expr)
{
    if (dynamic_cast<ThisNode*>(&expr))
    {
        return current_this_ptr;
    }
    if (auto* v = dynamic_cast<VarNameNode*>(&expr))
    {
        return allocas[v->decl];
    }
    if (auto* fa = dynamic_cast<FieldAccessNode*>(&expr))
    {
        llvm::Value* base_ptr = nullptr;
        if (dynamic_cast<MemberCallNode*>(fa->object.get()) || dynamic_cast<FunctionCallNode*>(fa->object.get()))
        {
            llvm::Value* val = fa->object->accept(*this);
            llvm::AllocaInst* tmp = create_entry_alloca(llvm_type(fa->object->type), "tmp_call_res");
            builder.CreateStore(val, tmp);
            base_ptr = tmp;
        }
        else
        {
            base_ptr = get_expr_address(*fa->object);
        }
        llvm::StructType* st = struct_types[fa->object->type];
        return builder.CreateStructGEP(st, base_ptr, fa->field_index);
    }
    llvm::Value* val = expr.accept(*this);
    llvm::AllocaInst* tmp = create_entry_alloca(llvm_type(expr.type), "tmp_obj");
    builder.CreateStore(val, tmp);
    return tmp;
}

void CodeGen::emit_print_struct(llvm::Value* struct_ptr, const std::string& type_name)
{
    const auto& sinfo = structs[type_name];
    llvm::StructType* st = struct_types[type_name];

    llvm::Value* open_fmt = builder.CreateGlobalStringPtr(type_name + "{", "fmt_st_open");
    builder.CreateCall(printf_func, {open_fmt});

    for (size_t i = 0; i < sinfo.fields.size(); ++i)
    {
        const auto& fi = sinfo.fields[i];
        llvm::Value* f_ptr = builder.CreateStructGEP(st, struct_ptr, fi.index);
        std::string prefix = (i > 0 ? ", " : "") + fi.name + ": ";
        llvm::Value* prefix_val = builder.CreateGlobalStringPtr(prefix, "fmt_fld");
        builder.CreateCall(printf_func, {prefix_val});

        if (fi.type == "i32")
        {
            llvm::Value* f_val = builder.CreateLoad(i32_type, f_ptr);
            llvm::Value* pfmt = builder.CreateGlobalStringPtr("%d", "fmt_d");
            builder.CreateCall(printf_func, {pfmt, f_val});
        }
        else if (fi.type == "i64")
        {
            llvm::Value* f_val = builder.CreateLoad(i64_type, f_ptr);
            llvm::Value* pfmt = builder.CreateGlobalStringPtr("%lld", "fmt_lld");
            builder.CreateCall(printf_func, {pfmt, f_val});
        }
        else if (fi.type == "bool")
        {
            llvm::Value* f_val = builder.CreateLoad(i1_type, f_ptr);
            llvm::Value* s = builder.CreateSelect(f_val, true_str, false_str);
            llvm::Value* pfmt = builder.CreateGlobalStringPtr("%s", "fmt_s");
            builder.CreateCall(printf_func, {pfmt, s});
        }
        else if (structs.count(fi.type))
        {
            emit_print_struct(f_ptr, fi.type);
        }
    }

    llvm::Value* close_fmt = builder.CreateGlobalStringPtr("}", "fmt_st_close");
    builder.CreateCall(printf_func, {close_fmt});
}

llvm::Value* CodeGen::visit_program(ProgramNode& n)
{
    // Step 1: Create struct types and record struct info
    for (auto& s : n.structs)
    {
        for (auto& m : s->member_fns)
            m->struct_decl = s.get();

        llvm::StructType* st = llvm::StructType::create(context, s->name);
        struct_types[s->name] = st;
        StructInfo info;
        info.name = s->name;
        for (size_t i = 0; i < s->fields.size(); ++i)
        {
            info.fields.push_back({s->fields[i]->type_name, s->fields[i]->name, s->fields[i]->is_mut, (int)i});
            info.field_index[s->fields[i]->name] = (int)i;
        }
        structs[s->name] = std::move(info);
    }

    // Step 2: Set struct bodies
    for (auto& s : n.structs)
    {
        std::vector<llvm::Type*> field_types;
        for (const auto& f : s->fields)
            field_types.push_back(llvm_type(f->type_name));
        struct_types[s->name]->setBody(field_types);
    }

    // Step 3: Declare member functions
    for (auto& s : n.structs)
    {
        for (auto& m : s->member_fns)
        {
            m->struct_decl = s.get();
            std::vector<llvm::Type*> param_types;
            param_types.push_back(llvm::PointerType::getUnqual(context)); // this
            for (const auto& p : m->params)
                param_types.push_back(llvm_type(p->type_name));

            llvm::Type* ret_t = llvm_type(m->return_type);
            llvm::FunctionType* ft = llvm::FunctionType::get(ret_t, param_types, false);
            std::string ir_name = member_fn_ir_name(s->name, m->name);
            llvm::Function* f = llvm::Function::Create(
                ft, llvm::Function::ExternalLinkage, ir_name, module.get());
            ir_member_functions[ir_name] = f;
        }
    }

    // Step 4: Declare global functions
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

    // Step 5: Define member functions
    for (auto& s : n.structs)
    {
        current_struct = s.get();
        for (auto& m : s->member_fns)
        {
            m->struct_decl = s.get();
            visit_member_fn_decl(*m);
        }
        current_struct = nullptr;
    }

    // Step 6: Define global functions
    for (auto& fn : n.functions)
        visit_fn_decl(*fn);

    // Step 7: Define main
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

    llvm::FunctionType* sft = llvm::FunctionType::get(
        i32_type, {llvm::PointerType::getUnqual(context)}, true);
    scanf_func = module->getOrInsertFunction("scanf", sft);

    read_slot = create_entry_alloca(i32_type, "read_slot");

    for (auto& s : n.stmts) s->accept(*this);
    emit_exit(*n.exit_stmt);

    return nullptr;
}

llvm::Value* CodeGen::visit_struct_decl(StructDeclNode&)
{
    return nullptr;
}

llvm::Value* CodeGen::visit_fn_decl(FunctionDeclNode& n)
{
    current_fn = &n;
    llvm::Function* f = ir_functions[n.name];

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", f);
    builder.SetInsertPoint(entry);

    allocas.clear();
    for (size_t i = 0; i < n.params.size(); ++i)
    {
        const auto& p = n.params[i];
        llvm::Type* pt = llvm_type(p->type_name);
        llvm::AllocaInst* a = create_entry_alloca(pt, p->name);
        builder.CreateStore(f->getArg(i), a);
        allocas[p->decl.get()] = a;
    }

    n.body->accept(*this);
    if (!current_block_terminated())
    {
        if (n.return_type == "bool")
            builder.CreateRet(llvm::ConstantInt::get(i1_type, 0));
        else if (n.return_type == "i64")
            builder.CreateRet(llvm::ConstantInt::get(i64_type, 0));
        else
            builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
    }

    current_fn = nullptr;
    return f;
}

llvm::Value* CodeGen::visit_member_fn_decl(MemberFnDeclNode& n)
{
    current_fn = &n;
    current_struct = n.struct_decl;
    std::string ir_name = member_fn_ir_name(current_struct->name, n.name);
    llvm::Function* f = ir_member_functions[ir_name];

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", f);
    builder.SetInsertPoint(entry);

    allocas.clear();
    current_this_ptr = f->getArg(0);

    for (size_t i = 0; i < n.params.size(); ++i)
    {
        const auto& p = n.params[i];
        llvm::Type* pt = llvm_type(p->type_name);
        llvm::AllocaInst* a = create_entry_alloca(pt, p->name);
        builder.CreateStore(f->getArg(i + 1), a);
        allocas[p->decl.get()] = a;
    }

    n.body->accept(*this);
    if (!current_block_terminated())
    {
        if (n.return_type == "bool")
            builder.CreateRet(llvm::ConstantInt::get(i1_type, 0));
        else if (n.return_type == "i64")
            builder.CreateRet(llvm::ConstantInt::get(i64_type, 0));
        else
            builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
    }

    current_fn = nullptr;
    current_struct = nullptr;
    current_this_ptr = nullptr;
    return f;
}

llvm::Value* CodeGen::visit_block(BlockNode& n)
{
    for (auto& s : n.stmts) s->accept(*this);
    if (n.exit_stmt) emit_exit(*n.exit_stmt);
    return nullptr;
}

llvm::Value* CodeGen::visit_decl(DeclNode& n)
{
    llvm::Type* t = llvm_type(n.type_name);
    llvm::AllocaInst* a = create_entry_alloca(t, n.name);
    allocas[&n] = a;

    bool is_builtin = (n.type_name == "i32" || n.type_name == "i64" || n.type_name == "bool");
    if (is_builtin)
    {
        llvm::Value* init_val = n.expr->items[0]->accept(*this);
        init_val = coerce(init_val, n.expr->items[0]->type, n.type_name);
        builder.CreateStore(init_val, a);
    }
    else
    {
        if (n.expr->items.size() == 1 && n.expr->items[0]->type == n.type_name)
        {
            llvm::Value* obj_val = n.expr->items[0]->accept(*this);
            builder.CreateStore(obj_val, a);
            return nullptr;
        }

        auto emit_init = [&](auto& self, llvm::Value* ptr, InitNode* inode, const std::string& tname) -> void {
            const auto& si = structs[tname];
            llvm::StructType* cur_st = struct_types[tname];
            for (size_t i = 0; i < si.fields.size(); ++i)
            {
                const auto& fi = si.fields[i];
                llvm::Value* f_ptr = builder.CreateStructGEP(cur_st, ptr, fi.index);
                if (auto* inner_init = dynamic_cast<InitNode*>(inode->items[i].get()))
                {
                    self(self, f_ptr, inner_init, fi.type);
                }
                else
                {
                    llvm::Value* f_val = inode->items[i]->accept(*this);
                    f_val = coerce(f_val, inode->items[i]->type, fi.type);
                    builder.CreateStore(f_val, f_ptr);
                }
            }
        };

        emit_init(emit_init, a, n.expr.get(), n.type_name);
    }
    return nullptr;
}

llvm::Value* CodeGen::visit_asmt(AsmtNode& n)
{
    llvm::Value* addr = get_expr_address(*n.lhs);
    llvm::Value* rhs = n.rhs->accept(*this);
    rhs = coerce(rhs, n.rhs->type, n.lhs->type);
    builder.CreateStore(rhs, addr);
    return nullptr;
}

void CodeGen::emit_exit(ExitNode& n)
{
    llvm::Value* v = n.expr->accept(*this);
    if (current_fn)
    {
        v = coerce(v, n.expr->type, current_fn->return_type);
        builder.CreateRet(v);
        return;
    }

    if (n.expr->type == "bool")
    {
        llvm::Value* s = builder.CreateSelect(v, true_str, false_str);
        builder.CreateCall(printf_func, {fmt_bool, s});
    }
    else
    {
        llvm::Value* v_i64 = coerce(v, n.expr->type, "i64");
        builder.CreateCall(printf_func, {fmt_int, v_i64});
    }
    builder.CreateRet(llvm::ConstantInt::get(i32_type, 0));
}

llvm::Value* CodeGen::visit_exit(ExitNode& n)
{
    emit_exit(n);
    return nullptr;
}

llvm::Value* CodeGen::visit_if(IfNode& n)
{
    llvm::Function* cur_f = current_fn ?
        (dynamic_cast<MemberFnDeclNode*>(current_fn) ?
            ir_member_functions[member_fn_ir_name(current_struct->name, current_fn->name)] :
            ir_functions[current_fn->name]) : main_func;

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
    llvm::Function* cur_f = current_fn ?
        (dynamic_cast<MemberFnDeclNode*>(current_fn) ?
            ir_member_functions[member_fn_ir_name(current_struct->name, current_fn->name)] :
            ir_functions[current_fn->name]) : main_func;

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

llvm::Value* CodeGen::visit_number(NumberNode& n)
{
    if (n.type == "i64")
        return llvm::ConstantInt::get(i64_type, n.value);
    return llvm::ConstantInt::get(i32_type, (int32_t)n.value);
}

llvm::Value* CodeGen::visit_bool(BoolNode& n)
{
    return llvm::ConstantInt::get(i1_type, n.value ? 1 : 0);
}

llvm::Value* CodeGen::visit_this(ThisNode&)
{
    return current_this_ptr;
}

llvm::Value* CodeGen::visit_var(VarNameNode& n)
{
    llvm::AllocaInst* a = allocas[n.decl];
    return builder.CreateLoad(llvm_type(n.decl->type_name), a);
}

llvm::Value* CodeGen::visit_not(NotNode& n)
{
    llvm::Value* v = n.expr->accept(*this);
    return builder.CreateNot(v, "not_tmp");
}

llvm::Value* CodeGen::visit_binop(BinaryOpNode& n)
{
    llvm::Value* l = n.left->accept(*this);
    llvm::Value* r = n.right->accept(*this);

    std::string common_type = "i32";
    if (n.left->type == "i64" || n.right->type == "i64") common_type = "i64";
    else if (n.left->type == "bool") common_type = "bool";

    l = coerce(l, n.left->type,  common_type);
    r = coerce(r, n.right->type, common_type);

    if (n.op == "+") return builder.CreateAdd(l, r, "add_tmp");
    if (n.op == "-") return builder.CreateSub(l, r, "sub_tmp");
    if (n.op == "*") return builder.CreateMul(l, r, "mul_tmp");
    if (n.op == "==") return builder.CreateICmpEQ(l, r, "eq_tmp");
    if (n.op == "!=") return builder.CreateICmpNE(l, r, "ne_tmp");

    throw std::runtime_error("unknown binary operator: " + n.op);
}

llvm::Value* CodeGen::visit_fn_call(FunctionCallNode& n)
{
    if (auto* mfn = dynamic_cast<MemberFnDeclNode*>(n.fn_decl))
    {
        std::string ir_name = member_fn_ir_name(current_struct->name, n.name);
        llvm::Function* callee = ir_member_functions[ir_name];
        std::vector<llvm::Value*> args;
        args.push_back(current_this_ptr);
        for (size_t i = 0; i < n.args.size(); ++i)
        {
            llvm::Value* av = n.args[i]->accept(*this);
            av = coerce(av, n.args[i]->type, mfn->params[i]->type_name);
            args.push_back(av);
        }
        return builder.CreateCall(callee, args, "member_call_tmp");
    }

    llvm::Function* callee = ir_functions[n.name];
    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < n.args.size(); ++i)
    {
        llvm::Value* av = n.args[i]->accept(*this);
        av = coerce(av, n.args[i]->type, n.fn_decl->params[i]->type_name);
        args.push_back(av);
    }
    return builder.CreateCall(callee, args, "call_tmp");
}

llvm::Value* CodeGen::visit_field_access(FieldAccessNode& n)
{
    if (dynamic_cast<MemberCallNode*>(n.object.get()) || dynamic_cast<FunctionCallNode*>(n.object.get()))
    {
        llvm::Value* val = n.object->accept(*this);
        return builder.CreateExtractValue(val, {(unsigned)n.field_index}, "ext_val");
    }
    llvm::Value* addr = get_expr_address(*n.object);
    llvm::StructType* st = struct_types[n.object->type];
    llvm::Value* gep = builder.CreateStructGEP(st, addr, n.field_index);
    return builder.CreateLoad(llvm_type(n.type), gep);
}

llvm::Value* CodeGen::visit_member_call(MemberCallNode& n)
{
    llvm::Value* this_arg = get_expr_address(*n.object);
    std::string ir_name = member_fn_ir_name(n.object->type, n.fn_name);
    llvm::Function* callee = ir_member_functions[ir_name];

    std::vector<llvm::Value*> args;
    args.push_back(this_arg);
    for (size_t i = 0; i < n.args.size(); ++i)
    {
        llvm::Value* av = n.args[i]->accept(*this);
        av = coerce(av, n.args[i]->type, n.fn->params[i]->type_name);
        args.push_back(av);
    }
    return builder.CreateCall(callee, args, "member_call_tmp");
}

llvm::Value* CodeGen::visit_print(PrintNode& n)
{
    if (n.is_string)
    {
        llvm::Value* str_val = builder.CreateGlobalStringPtr(n.string_literal, "str_lit");
        llvm::Value* pfmt = builder.CreateGlobalStringPtr("%s\n", "fmt_str_nl");
        builder.CreateCall(printf_func, {pfmt, str_val});
        return nullptr;
    }

    if (structs.count(n.expr->type))
    {
        llvm::Value* ptr = nullptr;
        if (dynamic_cast<MemberCallNode*>(n.expr.get()) || dynamic_cast<FunctionCallNode*>(n.expr.get()))
        {
            llvm::Value* val = n.expr->accept(*this);
            llvm::AllocaInst* tmp = create_entry_alloca(llvm_type(n.expr->type), "tmp_print_st");
            builder.CreateStore(val, tmp);
            ptr = tmp;
        }
        else
        {
            ptr = get_expr_address(*n.expr);
        }
        emit_print_struct(ptr, n.expr->type);
        llvm::Value* nl_fmt = builder.CreateGlobalStringPtr("\n", "fmt_nl");
        builder.CreateCall(printf_func, {nl_fmt});
        return nullptr;
    }

    llvm::Value* val = n.expr->accept(*this);
    if (n.expr->type == "i32")
    {
        llvm::Value* pfmt = builder.CreateGlobalStringPtr("%d\n", "fmt_d_nl");
        builder.CreateCall(printf_func, {pfmt, val});
    }
    else if (n.expr->type == "i64")
    {
        llvm::Value* pfmt = builder.CreateGlobalStringPtr("%lld\n", "fmt_lld_nl");
        builder.CreateCall(printf_func, {pfmt, val});
    }
    else if (n.expr->type == "bool")
    {
        llvm::Value* s = builder.CreateSelect(val, true_str, false_str);
        llvm::Value* pfmt = builder.CreateGlobalStringPtr("%s\n", "fmt_s_nl");
        builder.CreateCall(printf_func, {pfmt, s});
    }
    return nullptr;
}

llvm::Value* CodeGen::visit_read(ReadNode&)
{
    if (!read_slot)
        read_slot = create_entry_alloca(i32_type, "read_slot");
    llvm::Value* fmt = builder.CreateGlobalStringPtr("%d", "fmt_read");
    builder.CreateCall(scanf_func, {fmt, read_slot});
    return builder.CreateLoad(i32_type, read_slot);
}

llvm::Value* CodeGen::visit_init(InitNode& n)
{
    std::string cur_type = n.type;
    llvm::StructType* st = struct_types[cur_type];
    const auto& sinfo = structs[cur_type];
    llvm::Value* val = llvm::UndefValue::get(st);

    for (size_t i = 0; i < n.items.size(); ++i)
    {
        llvm::Value* item_val = n.items[i]->accept(*this);
        item_val = coerce(item_val, n.items[i]->type, sinfo.fields[i].type);
        val = builder.CreateInsertValue(val, item_val, {(unsigned)i});
    }
    return val;
}

#pragma once
#include <map>
#include <string>
#include <vector>

#include "ast.h"
#include "visitor.h"

struct FieldInfo {
    std::string type;
    std::string name;
    bool is_mut;
    int index;
};

struct StructInfo {
    std::string name;
    std::vector<FieldInfo> fields;
    std::map<std::string, int> field_index;
};

// Semantic analysis pass.
class SemanticChecker : public Visitor
{
public:
    SemanticChecker();

    llvm::Value* visit_program(ProgramNode& n) override;
    llvm::Value* visit_block(BlockNode& n)     override;
    llvm::Value* visit_decl(DeclNode& n)       override;
    llvm::Value* visit_asmt(AsmtNode& n)       override;
    llvm::Value* visit_exit(ExitNode& n)       override;
    llvm::Value* visit_if(IfNode& n)           override;
    llvm::Value* visit_while(WhileNode& n)     override;
    llvm::Value* visit_number(NumberNode& n)   override;
    llvm::Value* visit_bool(BoolNode& n)       override;
    llvm::Value* visit_var(VarNameNode& n)     override;
    llvm::Value* visit_not(NotNode& n)         override;
    llvm::Value* visit_binop(BinaryOpNode& n)  override;

    // Practice 6 visitors
    llvm::Value* visit_struct_decl(StructDeclNode& n)         override;
    llvm::Value* visit_fn_decl(FunctionDeclNode& n)           override;
    llvm::Value* visit_fn_call(FunctionCallNode& n)           override;
    llvm::Value* visit_field_access(StructFieldAccessNode& n) override;
    llvm::Value* visit_init(InitNode& n)                      override;

    std::map<std::string, StructInfo> structs;

private:
    using Scope = std::map<std::string, DeclNode*>;
    std::vector<Scope> scopes;
    std::map<std::string, FunctionDeclNode*> functions;
    FunctionDeclNode* current_fn = nullptr;

    [[noreturn]] void fail(int line, int col, const std::string& msg);
    [[noreturn]] void fail(const Node& at, const std::string& msg);

    DeclNode* lookup(const Node& at, const std::string& name);
    void check_assignable(const ExprNode& expr, const std::string& want,
                          const Node& at, const std::string& what);
    void check_condition(ExprNode& cond, const Node& at, const char* keyword);
    void check_init_item(ExprNode& item, const std::string& expected_type);
    bool all_fields_mut(const std::string& type_name);
};

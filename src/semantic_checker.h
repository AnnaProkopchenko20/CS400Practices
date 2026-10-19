#pragma once
#include <map>
#include <set>
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
    std::map<std::string, MemberFnDeclNode*> member_fns;
};

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
    llvm::Value* visit_this(ThisNode& n)       override;
    llvm::Value* visit_not(NotNode& n)         override;
    llvm::Value* visit_binop(BinaryOpNode& n)  override;
    llvm::Value* visit_struct_decl(StructDeclNode& n)       override;
    llvm::Value* visit_fn_decl(FunctionDeclNode& n)         override;
    llvm::Value* visit_fn_call(FunctionCallNode& n)         override;
    llvm::Value* visit_field_access(FieldAccessNode& n)     override;
    llvm::Value* visit_member_fn_decl(MemberFnDeclNode& n)  override;
    llvm::Value* visit_member_call(MemberCallNode& n)       override;
    llvm::Value* visit_print(PrintNode& n)                  override;
    llvm::Value* visit_read(ReadNode& n)                    override;
    llvm::Value* visit_init(InitNode& n)                    override;

    std::map<std::string, StructInfo> structs;

private:
    using Scope = std::map<std::string, DeclNode*>;
    std::vector<Scope> scopes;
    std::map<std::string, FunctionDeclNode*> functions;

    FunctionDeclNode* current_fn          = nullptr;
    StructDeclNode*   current_struct_node = nullptr;
    StructInfo*       current_struct_info = nullptr;
    MemberFnDeclNode* current_member_fn   = nullptr;

    bool in_member_body_pass = false;

    std::set<MemberFnDeclNode*> directly_mutates;
    std::map<MemberFnDeclNode*, std::set<MemberFnDeclNode*>> call_edges;

    struct DeferredCall {
        const Node* at;
        MemberFnDeclNode* target;
        bool chain_is_mut;
        bool has_const_field_link;
    };
    std::vector<DeferredCall> deferred_calls;

    [[noreturn]] void fail(int line, int col, const std::string& msg);
    [[noreturn]] void fail(const Node& at, const std::string& msg);

    bool is_builtin_type(const std::string& t);
    bool is_known_type(const std::string& t);
    void check_new_name(const Node& at, const std::string& name);

    struct LookupResult {
        enum Kind { NotFound, LocalVar, FieldOfThis };
        Kind kind = NotFound;
        DeclNode* decl = nullptr;
        int field_index = -1;
        std::string field_type;
        bool is_mut = false;
    };

    LookupResult lookup_name(const std::string& name);
    DeclNode* lookup(const Node& at, const std::string& name);

    void check_expr(std::unique_ptr<ExprNode>& slot);
    void maybe_rewrite_field(std::unique_ptr<ExprNode>& slot);
    void validate_lvalue(ExprNode& expr, const Node& at);

    bool is_chain_mut(const ExprNode& expr);
    bool has_const_field_link(const ExprNode& expr);
    bool is_rooted_at_this(const ExprNode& expr);

    void check_assignable(const ExprNode& expr, const std::string& want,
        const Node& at, const std::string& what);
    void check_condition(std::unique_ptr<ExprNode>& cond, const Node& at, const char* keyword);
    void check_init_item(std::unique_ptr<ExprNode>& item, const std::string& expected_type);
    bool all_fields_mut(const std::string& type_name);
};

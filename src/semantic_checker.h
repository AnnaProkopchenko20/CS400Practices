#pragma once
#include <map>
#include <string>
#include <vector>

#include "ast.h"

// Semantic analysis pass: resolves variable references, checks types and
// mutability.  Fills in .type on every ExprNode and the .decl back-links on
// VarNameNode and AsmtNode.
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

private:
    using Scope = std::map<std::string, DeclNode*>;
    std::vector<Scope> scopes;

    [[noreturn]] void fail(int line, int col, const std::string& msg);
    [[noreturn]] void fail(const Node& at, const std::string& msg);

    DeclNode* lookup(const Node& at, const std::string& name);
    void check_assignable(const ExprNode& expr, const std::string& want,
                          const Node& at, const std::string& what);
    void check_condition(ExprNode& cond, const Node& at, const char* keyword);
};

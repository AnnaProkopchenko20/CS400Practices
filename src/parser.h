#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include "ast.h"
#include "fn_param_node.h"
// lambda_node.h already included via ast.h
#include "keywords.h"
#include "lexer.h"

// Recursive-descent parser: one method per grammar rule.
// Tokens are presented grouped by source line; the end of a line is the NL terminal.
// Errors are thrown as std::runtime_error("line L:C: message").
class Parser
{
public:
    explicit Parser(const std::vector<std::vector<Token>>& lines);

    std::unique_ptr<ProgramNode> parse_program();

private:
    const std::vector<std::vector<Token>>& lines;
    std::vector<Token> toks;
    size_t pos      = 0;
    size_t next_idx = 0;

    const std::vector<Token>* peek_line() const;
    void next_line();

    const Token* peek() const;
    Token eat();
    bool at(const std::string& kind, const std::string& text = "") const;
    bool at_type() const;
    bool at_decl() const;
    Token expect_type();

    static std::string where(int line, int col);
    std::runtime_error error_at(const Token& t, const std::string& msg) const;
    std::runtime_error error(const std::string& msg) const;
    Token expect(const std::string& kind, const std::string& text, const std::string& what);
    void end_of_line();

    std::unique_ptr<StmtNode>         parse_statement();
    std::unique_ptr<StmtNode>         parse_decl();
    std::unique_ptr<StmtNode>         parse_assign();
    std::unique_ptr<StmtNode>         parse_print();
    std::unique_ptr<ExitNode>         parse_exit();
    std::unique_ptr<IfNode>           parse_if();
    std::unique_ptr<WhileNode>        parse_while();
    std::unique_ptr<BlockNode>        parse_block(const std::string& after);
    std::unique_ptr<ExprNode>         parse_expr();
    std::unique_ptr<ExprNode>         parse_arith();
    std::unique_ptr<ExprNode>         parse_term();
    std::unique_ptr<ExprNode>         parse_factor();
    std::unique_ptr<ExprNode>         parse_read();
    std::unique_ptr<ExprNode>         parse_identifier_chain();
    std::unique_ptr<ExprNode>         parse_member_call(std::unique_ptr<ExprNode> left, const Token& name, const Token& dot_tok);
    std::unique_ptr<ExprNode>         parse_field_access(std::unique_ptr<ExprNode> left, const Token& name, const Token& dot_tok);
    std::unique_ptr<StructDeclNode>   parse_struct_decl();
    std::unique_ptr<FieldDeclNode>    parse_field_decl();
    std::unique_ptr<FunctionDeclNode> parse_fn_decl();
    std::unique_ptr<MemberFnDeclNode> parse_member_fn();
    std::vector<std::unique_ptr<FuncParamNode>> parse_params();
    // Practice 8: lambda parsing
    bool at_lambda() const;
    std::unique_ptr<LambdaNode> parse_lambda();
    std::vector<std::unique_ptr<LambdaCapture>> parse_captures();
    std::vector<std::unique_ptr<ExprNode>>      parse_args();
    std::unique_ptr<FuncParamNode>    parse_param();
    std::unique_ptr<InitNode>         parse_init();
};

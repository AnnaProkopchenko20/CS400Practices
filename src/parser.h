#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include "ast.h"
#include "keywords.h"
#include "lexer.h"

// Recursive-descent parser: one method per grammar rule.
// Tokens are presented grouped by source line; the end of a line is the NL terminal.
//
// program     ::= { statement } exit_stmt
// statement   ::= declaration | assignment | if_stmt | while_stmt
// declaration ::= type [ "mut" ] identifier "{" expression "}"
// assignment  ::= identifier ":=" expression
// if_stmt     ::= "if" expression NL block [ "else" NL block ]
// while_stmt  ::= "while" expression NL block
// block       ::= "{" NL { statement } [ exit_stmt NL ] "}" NL
// exit_stmt   ::= "exit" factor
// type        ::= "i32" | "i64" | "bool"
// expression  ::= arith [ ( "==" | "!=" ) arith ]
// arith       ::= term { ( "+" | "-" ) term }
// term        ::= factor { "*" factor }
// factor      ::= number | "true" | "false" | identifier | "!" factor
//
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

    static std::string where(int line, int col);
    std::runtime_error error_at(const Token& t, const std::string& msg) const;
    std::runtime_error error(const std::string& msg) const;
    Token expect(const std::string& kind, const std::string& text, const std::string& what);
    void end_of_line();

    std::unique_ptr<StmtNode>  parse_statement();
    std::unique_ptr<StmtNode>  parse_decl();
    std::unique_ptr<StmtNode>  parse_assign();
    std::unique_ptr<ExitNode>  parse_exit();
    std::unique_ptr<IfNode>    parse_if();
    std::unique_ptr<WhileNode> parse_while();
    std::unique_ptr<BlockNode> parse_block(const std::string& after);
    std::unique_ptr<ExprNode>  parse_expr();
    std::unique_ptr<ExprNode>  parse_arith();
    std::unique_ptr<ExprNode>  parse_term();
    std::unique_ptr<ExprNode>  parse_factor();
};

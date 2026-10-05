#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "ast.h"
#include "lexer.h"

using std::string; using std::vector; using std::unique_ptr; using std::make_unique;

class Parser {
    const vector<vector<Token>>& lines;
    vector<Token> toks;
    size_t pos = 0;

public:
    explicit Parser(const vector<vector<Token>>& l) : lines(l) {}

    const Token* peek() const
    {
        return pos < toks.size() ? &toks[pos] : nullptr;
    }

    Token eat()
    {
        return toks[pos++];
    }

    bool at(const string& kind, const string& text = "")
    const {
        const Token* t = peek();
        return t && t->kind == kind && (text.empty() || t->text == text);
    }

    bool at_type() const
    {
        return at("keyword", "i32") || at("keyword", "i64") || at("keyword", "bool");
    }

    static string where(int line, int col)
    {
        return "line " + std::to_string(line) + ":" + std::to_string(col) + ": ";
    }

    std::runtime_error error_at(const Token& t, const string& msg)
    const {
        return std::runtime_error(where(t.line, t.col) + msg);
    }

    std::runtime_error error(const string& msg)
    const
    {
        if (const Token* t = peek())
        {
            return std::runtime_error(where(t->line, t->col) + msg + ", got '" + t->text + "'");
        }

        const Token& last = toks.back();
        return std::runtime_error(where(last.line, last.col + (int)last.text.size()) + msg + ", found end of line");
    }

    Token expect(const string& kind, const string& text, const string& what)
    {
        if (!at(kind, text)) throw error("expected " + what);
        return eat();
    }

    // program ::= { statement } exit_stmt
    unique_ptr<ProgramNode> parse_program()
    {
        vector<unique_ptr<StmtNode>> stmts;
        unique_ptr<ExitNode> exit_node;
        int first_line = 1, first_col = 1, last_line = 1;
        bool first = true;

        for (const auto& line : lines) {
            if (line.empty()) continue;

            toks = line;
            pos = 0;
            last_line = toks[0].line;
            if (first)
            {
                first_line = toks[0].line; first_col = toks[0].col;
                first = false;
            }

            if (exit_node)
            {
                throw error_at(toks[0], "statement after exit");
            }

            if (at("keyword", "exit")) exit_node = parse_exit();
            else stmts.push_back(parse_statement());

            if (peek())
                throw error_at(*peek(), "unexpected '" + peek()->text + "' after the statement");
        }
        if (!exit_node)
            throw std::runtime_error(where(last_line, 1) + "no exit");

        return make_unique<ProgramNode>(first_line, first_col, std::move(stmts), std::move(exit_node));
    }

    // statement ::= declaration | assignment
    unique_ptr<StmtNode> parse_statement() {
        if (at_type()) return parse_decl();
        if (at("identifier")) return parse_assign();

        throw error_at(*peek(), "cannot start a statement with '" + peek()->text + "'");
    }

    // declaration ::= type [ "mut" ] identifier "{" expression "}"
    unique_ptr<StmtNode> parse_decl() {
        Token type = eat();

        bool is_mut = at("keyword", "mut");
        if (is_mut) eat();

        Token name = expect("identifier", "", "a variable name");

        if (!at("block", "{")) {
            throw std::runtime_error(where(name.line, name.col + (int)name.text.size()) + "variable '" + name.text + "' needs an initialiser in {}");
        }

        eat();

        auto init = parse_expr();
        expect("block", "}", "'}'");

        return make_unique<DeclNode>(name.line, name.col, type.text, name.text, is_mut, std::move(init));
    }

    // assignment ::= identifier ":=" expression
    unique_ptr<StmtNode> parse_assign() {
        Token name = eat();

        if (!at("operator", ":="))
        {
            throw error("expected ':=' after '" + name.text + "'");
        }

        eat();
        auto value = parse_expr();

        return make_unique<AsmtNode>(name.line, name.col, name.text, std::move(value));
    }

    // exit_stmt ::= "exit" factor
    unique_ptr<ExitNode> parse_exit() {
        Token kw = eat();
        auto value = parse_factor();
        return make_unique<ExitNode>(kw.line, kw.col, std::move(value));
    }

    // expression  ::= arith [ ( "==" | "!=" ) arith ]
    unique_ptr<ExprNode> parse_expr()
    {
        unique_ptr<ExprNode> node = parse_arith();

        if (at("operator", "==") || at("operator", "!="))   // 'if', not 'while': one comparison only
        {
            Token op = eat();
            auto right = parse_arith();
            node = make_unique<BinaryOpNode>(op.line, op.col, op.text, std::move(node), std::move(right));
        }
        return node;
    }

    // arith ::= term { ( "+" | "-" ) term }
    unique_ptr<ExprNode> parse_arith()
    {
        unique_ptr<ExprNode> node = parse_term();

        while (at("operator", "+") || at("operator", "-"))
        {
            Token op = eat();
            auto right = parse_term();
            node = make_unique<BinaryOpNode>(op.line, op.col, op.text, std::move(node), std::move(right));
        }
        return node;
    }

    // term ::= factor { "*" factor }
    unique_ptr<ExprNode> parse_term()
    {
        unique_ptr<ExprNode> node = parse_factor();

        while (at("operator", "*"))
        {
            Token op = eat();
            auto right = parse_factor();
            node = make_unique<BinaryOpNode>(op.line, op.col, op.text, std::move(node), std::move(right));
        }

        return node;
    }

    // factor ::= number | "true" | "false" | identifier
    unique_ptr<ExprNode> parse_factor() {

        if (at("identifier")) {
            Token t = eat();
            return make_unique<VarNameNode>(t.line, t.col, t.text);
        }

        if (at("number")) {
            Token t = eat();
            return make_unique<NumberNode>(t.line, t.col, t.text);   // range is the checker's job
        }

        if (at("keyword", "true") || at("keyword", "false")) {
            Token t = eat();
            return make_unique<BoolNode>(t.line, t.col, t.text == "true");
        }

        throw error("expected a constant or a variable");
    }
};

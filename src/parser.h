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
    vector<Token> toks;       // the current line
    size_t pos = 0;           // token cursor inside the current line
    size_t next_idx = 0;      // line cursor: index of the first line not consumed yet

public:
    explicit Parser(const vector<vector<Token>>& l) : lines(l) {}

    // ---- line cursor (empty lines are skipped) ----

    const vector<Token>* peek_line() const
    {
        size_t j = next_idx;
        while (j < lines.size() && lines[j].empty()) j++;
        return j < lines.size() ? &lines[j] : nullptr;
    }

    // makes the next non-empty line the current one; call only if peek_line() != nullptr
    void next_line()
    {
        while (next_idx < lines.size() && lines[next_idx].empty()) next_idx++;
        toks = lines[next_idx++];
        pos = 0;
    }

    // ---- token cursor ----

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

    // ---- errors ----

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

    // the current line must be fully consumed
    void end_of_line()
    {
        if (const Token* t = peek())
            throw error_at(*t, "unexpected '" + t->text + "' after the statement");
    }

    // ---- grammar ----

    // program ::= { statement } exit_stmt
    unique_ptr<ProgramNode> parse_program()
    {
        vector<unique_ptr<StmtNode>> stmts;
        unique_ptr<ExitNode> exit_node;
        int first_line = 1, first_col = 1, last_line = 1;

        if (const vector<Token>* f = peek_line())
        {
            first_line = (*f)[0].line;
            first_col = (*f)[0].col;
        }

        while (peek_line())
        {
            next_line();
            last_line = toks[0].line;

            if (exit_node)
                throw error_at(toks[0], "statement after exit");

            if (at("keyword", "exit"))
            {
                exit_node = parse_exit();
                end_of_line();
            }
            else
            {
                stmts.push_back(parse_statement());
            }
        }

        if (!exit_node)
            throw std::runtime_error(where(last_line, 1) + "no exit");

        return make_unique<ProgramNode>(first_line, first_col, std::move(stmts), std::move(exit_node));
    }

    // statement ::= decl | assign | if | while
    // consumes one line, or for if / while the whole construct
    unique_ptr<StmtNode> parse_statement()
    {
        if (at_type())
        {
            auto s = parse_decl();
            end_of_line();
            return s;
        }
        if (at("identifier"))
        {
            auto s = parse_assign();
            end_of_line();
            return s;
        }
        if (at("keyword", "if")) return parse_if();
        if (at("keyword", "while")) return parse_while();
        if (at("keyword", "else"))
            throw error_at(*peek(), "'else' without an 'if'");

        throw error_at(*peek(), "cannot start a statement with '" + peek()->text + "'");
    }

    // factor ::= number | "true" | "false" | ident | "!" factor
    unique_ptr<ExprNode> parse_factor()
    {
        if (at("operator", "!"))
        {
            Token t = eat();
            auto operand = parse_factor();
            return make_unique<NotNode>(t.line, t.col, std::move(operand));
        }

        if (at("identifier"))
        {
            Token t = eat();
            return make_unique<VarNameNode>(t.line, t.col, t.text);
        }

        if (at("number"))
        {
            Token t = eat();
            return make_unique<NumberNode>(t.line, t.col, t.text);
        }

        if (at("keyword", "true") || at("keyword", "false"))
        {
            Token t = eat();
            return make_unique<BoolNode>(t.line, t.col, t.text == "true");
        }

        throw error("expected a constant, variable, or '!'");
    }

    // declaration ::= type [ "mut" ] identifier "{" expression "}"
    unique_ptr<StmtNode> parse_decl()
    {
        Token type = eat();

        bool is_mut = at("keyword", "mut");
        if (is_mut) eat();

        Token name = expect("identifier", "", "a variable name");

        if (!at("block", "{"))
        {
            throw std::runtime_error(where(name.line, name.col + (int)name.text.size()) + "variable '" + name.text + "' needs an initialiser in {}");
        }

        eat();

        auto init = parse_expr();
        expect("block", "}", "'}'");

        return make_unique<DeclNode>(name.line, name.col, type.text, name.text, is_mut, std::move(init));
    }

    // assignment ::= identifier ":=" expression
    unique_ptr<StmtNode> parse_assign()
    {
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
    unique_ptr<ExitNode> parse_exit()
    {
        Token kw = eat();
        auto value = parse_factor();
        return make_unique<ExitNode>(kw.line, kw.col, std::move(value));
    }

    // expression ::= arith [ ( "==" | "!=" ) arith ]
    unique_ptr<ExprNode> parse_expr()
    {
        unique_ptr<ExprNode> node = parse_arith();

        if (at("operator", "==") || at("operator", "!="))
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

    // if ::= "if" expr NL block [ "else" NL block ]
    unique_ptr<IfNode> parse_if()
    {
        Token kw = eat();
        auto cond = parse_expr();
        end_of_line();                       // 'if b {' ends here

        auto then_block = parse_block("if");
        unique_ptr<BlockNode> else_block;

        const vector<Token>* nl = peek_line();
        if (nl && (*nl)[0].kind == "keyword" && (*nl)[0].text == "else")
        {
            next_line();
            eat();                           // 'else'
            end_of_line();
            else_block = parse_block("else");
        }

        return make_unique<IfNode>(kw.line, kw.col, std::move(cond), std::move(then_block), std::move(else_block));
    }

    // while ::= "while" expr NL block
    unique_ptr<WhileNode> parse_while()
    {
        Token kw = eat();
        auto cond = parse_expr();
        end_of_line();

        auto body = parse_block("while");

        return make_unique<WhileNode>(kw.line, kw.col, std::move(cond), std::move(body));
    }

    // block ::= "{" NL { statement } [ exit NL ] "}" NL   (not empty)
    // 'after' is the keyword the block belongs to, for the error message
    unique_ptr<BlockNode> parse_block(const string& after)
    {
        const vector<Token>* nl = peek_line();

        if (!nl)
        {
            throw std::runtime_error(where(toks.back().line + 1, 1) + "expected '{' on its own line after '" + after + "', found end of file");
        }
        if (!((*nl)[0].kind == "block" && (*nl)[0].text == "{"))
        {
            throw error_at((*nl)[0], "expected '{' on its own line after '" + after + "', got '" + (*nl)[0].text + "'");
        }

        next_line();
        Token open = eat();
        end_of_line();

        vector<unique_ptr<StmtNode>> stmts;
        unique_ptr<ExitNode> exit_node;

        while (true)
        {
            if (!peek_line())
                throw error_at(open, "'{' is never closed");

            next_line();

            if (at("block", "}"))
            {
                eat();
                end_of_line();

                if (stmts.empty() && !exit_node)
                    throw error_at(open, "empty block");

                return make_unique<BlockNode>(open.line, open.col, std::move(stmts), std::move(exit_node));
            }

            if (exit_node)
                throw error_at(toks[0], "statement after 'exit' in the same block");

            if (at("keyword", "exit"))
            {
                exit_node = parse_exit();
                end_of_line();
            }
            else
            {
                stmts.push_back(parse_statement());
            }
        }
    }
};

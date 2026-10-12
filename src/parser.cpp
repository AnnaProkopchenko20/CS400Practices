#include "parser.h"
#include <stdexcept>

// ---- line cursor ----

const std::vector<Token>* Parser::peek_line() const
{
    size_t j = next_idx;
    while (j < lines.size() && lines[j].empty()) { j++; }
    return j < lines.size() ? &lines[j] : nullptr;
}

void Parser::next_line()
{
    while (next_idx < lines.size() && lines[next_idx].empty()) { next_idx++; }
    toks = lines[next_idx++];
    pos  = 0;
}

// ---- token cursor ----

const Token* Parser::peek() const
{
    return pos < toks.size() ? &toks[pos] : nullptr;
}

Token Parser::eat()
{
    return toks[pos++];
}

bool Parser::at(const std::string& kind, const std::string& text) const
{
    const Token* t = peek();
    return t && t->kind == kind && (text.empty() || t->text == text);
}

bool Parser::at_type() const
{
    return at("keyword", kw::I32) || at("keyword", kw::I64) || at("keyword", kw::BOOL);
}

// ---- errors ----

std::string Parser::where(int line, int col)
{
    return "line " + std::to_string(line) + ":" + std::to_string(col) + ": ";
}

std::runtime_error Parser::error_at(const Token& t, const std::string& msg) const
{
    return std::runtime_error(where(t.line, t.col) + msg);
}

std::runtime_error Parser::error(const std::string& msg) const
{
    if (const Token* t = peek())
        return std::runtime_error(where(t->line, t->col) + msg + ", got '" + t->text + "'");
    const Token& last = toks.back();
    return std::runtime_error(
        where(last.line, last.col + (int)last.text.size()) + msg + ", found end of line");
}

Token Parser::expect(const std::string& kind, const std::string& text, const std::string& what)
{
    if (!at(kind, text)) { throw error("expected " + what); }
    return eat();
}

void Parser::end_of_line()
{
    if (const Token* t = peek())
        throw error_at(*t, "unexpected '" + t->text + "' after the statement");
}

// ---- grammar ----

Parser::Parser(const std::vector<std::vector<Token>>& l) : lines(l) {}

// program ::= { statement } exit_stmt
std::unique_ptr<ProgramNode> Parser::parse_program()
{
    std::vector<std::unique_ptr<StmtNode>> stmts;
    std::unique_ptr<ExitNode> exit_node;
    int first_line = 1, first_col = 1, last_line = 1;

    if (const std::vector<Token>* f = peek_line())
    {
        first_line = (*f)[0].line;
        first_col  = (*f)[0].col;
    }

    while (peek_line())
    {
        next_line();
        last_line = toks[0].line;

        if (exit_node)
            throw error_at(toks[0], "statement after exit");

        if (at("keyword", kw::EXIT))
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

    return make_unique<ProgramNode>(first_line, first_col,
                                   std::move(stmts), std::move(exit_node));
}

// statement ::= declaration | assignment | if_stmt | while_stmt
std::unique_ptr<StmtNode> Parser::parse_statement()
{
    if (at_type())              { auto s = parse_decl();   end_of_line(); return s; }
    if (at("identifier"))       { auto s = parse_assign(); end_of_line(); return s; }
    if (at("keyword", kw::IF))  { return parse_if(); }
    if (at("keyword", kw::WHILE)){ return parse_while(); }
    if (at("keyword", kw::ELSE))
        throw error_at(*peek(), "'else' without an 'if'");
    throw error_at(*peek(), "cannot start a statement with '" + peek()->text + "'");
}

// declaration ::= type [ "mut" ] identifier "{" expression "}"
std::unique_ptr<StmtNode> Parser::parse_decl()
{
    Token type = eat();
    bool is_mut = at("keyword", kw::MUT);
    if (is_mut) { eat(); }
    Token name = expect("identifier", "", "a variable name");
    if (!at("block", "{"))
        throw std::runtime_error(
            where(name.line, name.col + (int)name.text.size()) +
            "variable '" + name.text + "' needs an initialiser in {}");
    eat();
    auto init = parse_expr();
    expect("block", "}", "'}'");
    return make_unique<DeclNode>(name.line, name.col,
                                 type.text, name.text, is_mut, std::move(init));
}

// assignment ::= identifier ":=" expression
std::unique_ptr<StmtNode> Parser::parse_assign()
{
    Token name = eat();
    if (!at("operator", ":="))
        throw error("expected ':=' after '" + name.text + "'");
    eat();
    auto value = parse_expr();
    return make_unique<AsmtNode>(name.line, name.col, name.text, std::move(value));
}

// exit_stmt ::= "exit" factor
std::unique_ptr<ExitNode> Parser::parse_exit()
{
    Token kw_tok = eat();
    auto value   = parse_factor();
    return make_unique<ExitNode>(kw_tok.line, kw_tok.col, std::move(value));
}

// expression ::= arith [ ( "==" | "!=" ) arith ]
std::unique_ptr<ExprNode> Parser::parse_expr()
{
    auto node = parse_arith();
    if (at("operator", "==") || at("operator", "!="))
    {
        Token op = eat();
        auto right = parse_arith();
        node = make_unique<BinaryOpNode>(op.line, op.col, op.text,
                                         std::move(node), std::move(right));
    }
    return node;
}

// arith ::= term { ( "+" | "-" ) term }
std::unique_ptr<ExprNode> Parser::parse_arith()
{
    auto node = parse_term();
    while (at("operator", "+") || at("operator", "-"))
    {
        Token op = eat();
        auto right = parse_term();
        node = make_unique<BinaryOpNode>(op.line, op.col, op.text,
                                         std::move(node), std::move(right));
    }
    return node;
}

// term ::= factor { "*" factor }
std::unique_ptr<ExprNode> Parser::parse_term()
{
    auto node = parse_factor();
    while (at("operator", "*"))
    {
        Token op = eat();
        auto right = parse_factor();
        node = make_unique<BinaryOpNode>(op.line, op.col, op.text,
                                         std::move(node), std::move(right));
    }
    return node;
}

// factor ::= number | "true" | "false" | identifier | "!" factor
std::unique_ptr<ExprNode> Parser::parse_factor()
{
    if (at("operator", "!"))
    {
        Token t = eat();
        return make_unique<NotNode>(t.line, t.col, parse_factor());
    }
    if (at("identifier"))  { Token t = eat(); return make_unique<VarNameNode>(t.line, t.col, t.text); }
    if (at("number"))      { Token t = eat(); return make_unique<NumberNode>(t.line, t.col, t.text); }
    if (at("keyword", kw::TRUE_) || at("keyword", kw::FALSE_))
    {
        Token t = eat();
        return make_unique<BoolNode>(t.line, t.col, t.text == kw::TRUE_);
    }
    throw error("expected a constant, variable, or '!'");
}

// if_stmt ::= "if" expression NL block [ "else" NL block ]
std::unique_ptr<IfNode> Parser::parse_if()
{
    Token kw_tok = eat();
    auto  cond   = parse_expr();
    end_of_line();

    auto then_block = parse_block(kw::IF);
    std::unique_ptr<BlockNode> else_block;

    const std::vector<Token>* nl = peek_line();
    if (nl && (*nl)[0].kind == "keyword" && (*nl)[0].text == kw::ELSE)
    {
        next_line();
        eat(); end_of_line();
        else_block = parse_block(kw::ELSE);
    }

    return make_unique<IfNode>(kw_tok.line, kw_tok.col,
                               std::move(cond), std::move(then_block), std::move(else_block));
}

// while_stmt ::= "while" expression NL block
std::unique_ptr<WhileNode> Parser::parse_while()
{
    Token kw_tok = eat();
    auto  cond   = parse_expr();
    end_of_line();
    return make_unique<WhileNode>(kw_tok.line, kw_tok.col,
                                  std::move(cond), parse_block(kw::WHILE));
}

// block ::= "{" NL { statement } [ exit_stmt NL ] "}" NL
std::unique_ptr<BlockNode> Parser::parse_block(const std::string& after)
{
    const std::vector<Token>* nl = peek_line();
    if (!nl)
        throw std::runtime_error(
            where(toks.back().line + 1, 1) +
            "expected '{' on its own line after '" + after + "', found end of file");
    if (!((*nl)[0].kind == "block" && (*nl)[0].text == "{"))
        throw error_at((*nl)[0],
            "expected '{' on its own line after '" + after + "', got '" + (*nl)[0].text + "'");

    next_line();
    Token open = eat();
    end_of_line();

    std::vector<std::unique_ptr<StmtNode>> stmts;
    std::unique_ptr<ExitNode> exit_node;

    while (true)
    {
        if (!peek_line())
            throw error_at(open, "'{' is never closed");
        next_line();

        if (at("block", "}"))
        {
            eat(); end_of_line();
            if (stmts.empty() && !exit_node)
                throw error_at(open, "empty block");
            return make_unique<BlockNode>(open.line, open.col,
                                          std::move(stmts), std::move(exit_node));
        }

        if (exit_node)
            throw error_at(toks[0], "statement after 'exit' in the same block");

        if (at("keyword", kw::EXIT))
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

#include "parser.h"

const std::vector<Token>* Parser::peek_line() const
{
    size_t idx = next_idx;
    while (idx < lines.size() && lines[idx].empty())
        idx++;
    return idx < lines.size() ? &lines[idx] : nullptr;
}

void Parser::next_line()
{
    while (next_idx < lines.size() && lines[next_idx].empty())
        next_idx++;
    if (next_idx < lines.size())
        toks = lines[next_idx++];
    else
        toks.clear();
    pos  = 0;
}

const Token* Parser::peek() const
{
    return pos < toks.size() ? &toks[pos] : nullptr;
}

Token Parser::eat()
{
    if (const Token* t = peek()) { pos += 1; return *t; }
    throw error("unexpected end of line");
}

bool Parser::at(const std::string& kind, const std::string& text) const
{
    const Token* t = peek();
    if (!t || t->kind != kind) return false;
    return text.empty() || t->text == text;
}

bool Parser::at_type() const
{
    return at("keyword", kw::I32) ||
        at("keyword", kw::I64) ||
        at("keyword", kw::BOOL) ||
        at("identifier");
}

bool Parser::at_decl() const
{
    if (at("keyword", kw::I32) || at("keyword", kw::I64) || at("keyword", kw::BOOL))
        return true;
    if (at("identifier") && pos + 1 < toks.size())
    {
        const auto& next = toks[pos + 1];
        if (next.kind == "keyword" && next.text == kw::MUT)
            return true;
        if (next.kind == "identifier")
            return true;
    }
    return false;
}

Token Parser::expect_type()
{
    if (!at_type())
        throw error("expected a type");
    return eat();
}

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

Parser::Parser(const std::vector<std::vector<Token>>& l) : lines(l) {}

enum class ProgramSection { Structs, Functions, Statements, Exit };

// program ::= { struct } { fn } { statement } exit_stmt
std::unique_ptr<ProgramNode> Parser::parse_program()
{
    ProgramSection section = ProgramSection::Structs;
    std::vector<std::unique_ptr<StructDeclNode>>   structs;
    std::vector<std::unique_ptr<FunctionDeclNode>> functions;
    std::vector<std::unique_ptr<StmtNode>>         stmts;
    std::unique_ptr<ExitNode>                      exit_node;
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

        if (section == ProgramSection::Exit)
            throw error_at(toks[0], "statement after exit");

        if (at("keyword", kw::STRUCT))
        {
            if (section > ProgramSection::Structs)
                throw error_at(toks[0], "struct declarations come before functions");
            structs.push_back(parse_struct_decl());
        }
        else if (at("keyword", kw::FN))
        {
            if (section > ProgramSection::Functions)
                throw error_at(toks[0], "function declarations come before the statements");
            section = ProgramSection::Functions;
            functions.push_back(parse_fn_decl());
        }
        else if (at("keyword", kw::EXIT))
        {
            section = ProgramSection::Exit;
            exit_node = parse_exit();
            end_of_line();
        }
        else
        {
            section = ProgramSection::Statements;
            stmts.push_back(parse_statement());
        }
    }

    if (!exit_node)
        throw std::runtime_error(where(last_line, 1) + "no exit");

    return make_unique<ProgramNode>(
        first_line, first_col,
        std::move(structs), std::move(functions),
        std::move(stmts), std::move(exit_node));
}

// struct ::= "struct" ident NL "{" NL field_decl { field_decl } "}" NL
std::unique_ptr<StructDeclNode> Parser::parse_struct_decl()
{
    Token kw_tok = eat();
    Token name   = expect("identifier", "", "a struct name");
    end_of_line();

    const std::vector<Token>* nl = peek_line();
    if (!nl)
        throw std::runtime_error(
            where(toks.back().line + 1, 1) +
            "expected '{' on its own line after struct '" + name.text + "', found end of file");
    if (!((*nl)[0].kind == "block" && (*nl)[0].text == "{"))
        throw error_at((*nl)[0],
            "expected '{' on its own line after struct '" + name.text + "', got '" + (*nl)[0].text + "'");

    next_line();
    Token open = eat();
    end_of_line();

    std::vector<std::unique_ptr<FieldDeclNode>> fields;
    while (true)
    {
        if (!peek_line())
            throw error_at(open, "'{' is never closed");
        next_line();

        if (at("block", "}"))
        {
            eat(); end_of_line();
            if (fields.empty())
                throw error_at(open, "a struct must have at least one field");
            return make_unique<StructDeclNode>(
                kw_tok.line, kw_tok.col,
                name.text, std::move(fields));
        }

        fields.push_back(parse_field_decl());
    }
}

// field_decl ::= type [ "mut" ] ident NL
std::unique_ptr<FieldDeclNode> Parser::parse_field_decl()
{
    Token type_tok = expect_type();
    bool is_mut = at("keyword", kw::MUT);
    if (is_mut) eat();
    Token name_tok = expect("identifier", "", "a field name");
    end_of_line();
    return make_unique<FieldDeclNode>(
        type_tok.line, type_tok.col,
        type_tok.text, name_tok.text, is_mut);
}

// fn ::= "fn" ident ":=" "(" [ param { "," param } ] ")" "->" type NL block
std::unique_ptr<FunctionDeclNode> Parser::parse_fn_decl()
{
    Token kw_tok = eat();
    Token name   = expect("identifier", "", "a function name");
    expect("operator", ":=", "':='");

    if (!at("parentheses", "("))
        throw error("expected '('");
    eat();

    std::vector<std::unique_ptr<FuncParamNode>> params;
    if (!at("parentheses", ")"))
    {
        while (true)
        {
            params.push_back(parse_param());
            if (at("comma", ","))
                eat();
            else
                break;
        }
    }
    if (!at("parentheses", ")"))
        throw error("expected ')'");
    eat();

    expect("operator", "->", "'->'");
    Token ret_type = expect_type();
    end_of_line();

    auto body = parse_block(kw::FN);
    if (!body->exit_stmt)
        throw error_at(kw_tok, "function '" + name.text + "' must end with 'exit'");

    return make_unique<FunctionDeclNode>(
        kw_tok.line, kw_tok.col,
        name.text, ret_type.text,
        std::move(params), std::move(body));
}

// param ::= type ident
std::unique_ptr<FuncParamNode> Parser::parse_param()
{
    Token type_tok = expect_type();
    Token name_tok = expect("identifier", "", "a parameter name");
    return make_unique<FuncParamNode>(
        type_tok.line, type_tok.col,
        type_tok.text, name_tok.text);
}

// statement ::= declaration | assignment | if_stmt | while_stmt
std::unique_ptr<StmtNode> Parser::parse_statement()
{
    if (at_decl())              { auto s = parse_decl();   end_of_line(); return s; }
    if (at("identifier"))       { auto s = parse_assign(); end_of_line(); return s; }
    if (at("keyword", kw::IF))  { return parse_if(); }
    if (at("keyword", kw::WHILE)){ return parse_while(); }
    if (at("keyword", kw::ELSE))
        throw error_at(*peek(), "'else' without an 'if'");
    throw error_at(*peek(), "cannot start a statement with '" + peek()->text + "'");
}

// init ::= "{" value { "," value } "}"
// value ::= expression | init
std::unique_ptr<InitNode> Parser::parse_init()
{
    Token open = expect("block", "{", "'{'");
    std::vector<std::unique_ptr<ExprNode>> items;

    while (!at("block", "}"))
    {
        if (at("block", "{"))
            items.push_back(parse_init());
        else
            items.push_back(parse_expr());

        if (at("comma", ","))
            eat();
        else
            break;
    }

    expect("block", "}", "'}'");
    return make_unique<InitNode>(open.line, open.col, std::move(items));
}

// declaration ::= type [ "mut" ] identifier init
std::unique_ptr<StmtNode> Parser::parse_decl()
{
    Token type = expect_type();
    bool is_mut = at("keyword", kw::MUT);
    if (is_mut) { eat(); }
    Token name = expect("identifier", "", "a variable name");
    if (!at("block", "{"))
        throw std::runtime_error(
            where(name.line, name.col) +
            "variable '" + name.text + "' needs an initialiser in {}");
    auto init = parse_init();
    return make_unique<DeclNode>(
        name.line, name.col,
        type.text, name.text, is_mut, std::move(init));
}

// assignment ::= identifier { "." identifier } ":=" expression
std::unique_ptr<StmtNode> Parser::parse_assign()
{
    Token name = eat();
    std::vector<std::string> fields;
    while (at("dot", "."))
    {
        eat();
        Token f = expect("identifier", "", "a field name");
        fields.push_back(f.text);
    }

    if (!at("operator", ":="))
        throw error("expected ':=' after '" + name.text + "'");
    eat();

    auto value = parse_expr();
    if (fields.empty())
        return make_unique<AsmtNode>(name.line, name.col, name.text, std::move(value));
    return make_unique<AsmtNode>(name.line, name.col, name.text, std::move(fields), std::move(value));
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
        node = make_unique<BinaryOpNode>(
            op.line, op.col, op.text,
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
        node = make_unique<BinaryOpNode>(
            op.line, op.col, op.text,
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
        node = make_unique<BinaryOpNode>(
            op.line, op.col, op.text,
            std::move(node), std::move(right));
    }
    return node;
}

// factor ::= number | "true" | "false" | identifier | field | call | "!" factor
std::unique_ptr<ExprNode> Parser::parse_factor()
{
    if (at("operator", "!"))
    {
        Token t = eat();
        return make_unique<NotNode>(t.line, t.col, parse_factor());
    }
    if (at("identifier"))
    {
        Token t = eat();
        if (at("parentheses", "("))
        {
            eat();
            std::vector<std::unique_ptr<ExprNode>> args;
            if (!at("parentheses", ")"))
            {
                while (true)
                {
                    args.push_back(parse_expr());
                    if (at("comma", ","))
                        eat();
                    else
                        break;
                }
            }
            expect("parentheses", ")", "')'");
            return make_unique<FunctionCallNode>(t.line, t.col, t.text, std::move(args));
        }
        if (at("dot", "."))
        {
            std::vector<std::string> fields;
            while (at("dot", "."))
            {
                eat();
                Token f = expect("identifier", "", "a field name");
                fields.push_back(f.text);
            }
            return make_unique<StructFieldAccessNode>(t.line, t.col, t.text, std::move(fields));
        }
        return make_unique<VarNameNode>(t.line, t.col, t.text);
    }
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

    return make_unique<IfNode>(
        kw_tok.line, kw_tok.col,
        std::move(cond), std::move(then_block), std::move(else_block));
}

// while_stmt ::= "while" expression NL block
std::unique_ptr<WhileNode> Parser::parse_while()
{
    Token kw_tok = eat();
    auto  cond   = parse_expr();
    end_of_line();
    return make_unique<WhileNode>(
        kw_tok.line, kw_tok.col,
        std::move(cond), parse_block(kw::WHILE));
}

// block ::= "{" NL { statement } [ exit_stmt NL ] "}" NL
std::unique_ptr<BlockNode> Parser::parse_block(const std::string& after) {
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
            return make_unique<BlockNode>(
                open.line, open.col,
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

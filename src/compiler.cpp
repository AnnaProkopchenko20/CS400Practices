#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <system_error>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <memory>
using std::string; using std::vector; using std::unique_ptr; using std::make_unique;

struct Token
{
    std::string kind;
    std::string text;
    int line;
    int col;
};

struct VariableInfo
{
    llvm::AllocaInst* alloca;
    bool is_mut;
};

std::vector<std::vector<Token>> lex(const std::string& data)
{
    std::vector<std::vector<Token>> lines;
    std::vector<Token> tokens;

    enum class State { START, IDENT, NUMBER, ASSIGNMENT };
    State state = State::START;

    int start = 0, line = 1, col = 1;
    size_t i = 0;

    int open_brace_col = -1;

    std::map<std::string, std::string> keywords =
    {
        {"i32", "keyword"},
        {"mut", "keyword"},
        {"exit", "keyword"}
    };

    while (i <= data.length())
    {
        bool is_eof = (i == data.length());
        unsigned char b = is_eof ? 0 : data[i];

        if (!is_eof && b > 127)
        {
            throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col) + ": unexpected byte");
        }

        if (state == State::START)
        {
            if (is_eof) {
                if (open_brace_col != -1)
                {
                    throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(open_brace_col) + ": '{' is not closed before the end of the line");
                }
                break;
            }
            else if (b == ' ' || b == '\t')
            {
                // pass
            }
            else if (std::isalpha(b))
            {
                state = State::IDENT;
                start = i;
            }
            else if (b == '\n')
            {
                if (open_brace_col != -1)
                {
                    throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(open_brace_col) + ": '{' is not closed before the end of the line");
                }

                lines.push_back(tokens);
                tokens.clear();
                line += 1;
                col = 0;
            }
            else if (std::isdigit(b))
            {
                state = State::NUMBER;
                start = i;
            }
            else if (b == '{')
            {
                tokens.push_back({"block", "{", line, col});
                open_brace_col = col;
            }
            else if (b == '}')
            {
                tokens.push_back({"block", "}", line, col});
                open_brace_col = -1;
            }
            else if (b == '+' || b == '-' || b == '*')
            {
                tokens.push_back({"operator", std::string(1, b), line, col});
            }
            else if (b == ':')
            {
                state = State::ASSIGNMENT;
                start = i;
            }
            else
            {
                throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col) + ": unexpected byte '" + std::string(1, b) + "'");
            }
        }
        else if (state == State::IDENT)
        {
            if (!is_eof && (std::isalpha(b) || std::isdigit(b) || b == '_'))
            {
                // pass
            } else
            {
                std::string word = data.substr(start, i - start);
                std::string kind = keywords.count(word) ? keywords[word] : "identifier";
                tokens.push_back({kind, word, line, col - (int)(i - start)});
                state = State::START;
                continue;
            }
        }
        else if (state == State::NUMBER)
        {
            if (!is_eof && std::isdigit(b))
            {
                // pass
            }
            else if (!is_eof && std::isalpha(b))
            {
                throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col) + ": unexpected letter inside a number");
            }
            else {
                std::string word = data.substr(start, i - start);
                tokens.push_back({"number", word, line, col - (int)(i - start)});
                state = State::START;
                continue;
            }
        }
        else if (state == State::ASSIGNMENT)
        {
            if (b == '=')
            {
                tokens.push_back({"operator", ":=", line, col - 1});
                state = State::START;
            }
            else
            {
                throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col - 1) + ": ':' not followed by '='");
            }
        }

        i += 1;
        col += 1;
    }

    if (!tokens.empty())
    {
        lines.push_back(tokens);
    }

    return lines;
}

struct ProgramNode; struct DeclNode; struct AsmtNode; struct ExitNode;
struct NumberNode; struct VarNameNode; struct BinaryOpNode;

struct Visitor {
    virtual ~Visitor() = default;
    virtual llvm::Value* visit_program(ProgramNode&) = 0;
    virtual llvm::Value* visit_decl(DeclNode&) = 0;
    virtual llvm::Value* visit_asmt(AsmtNode&) = 0;
    virtual llvm::Value* visit_exit(ExitNode&) = 0;
    virtual llvm::Value* visit_number(NumberNode&) = 0;
    virtual llvm::Value* visit_var(VarNameNode&) = 0;
    virtual llvm::Value* visit_binop(BinaryOpNode&) = 0;
};



struct Node {
    int line, col;
    Node(int l, int c) : line(l), col(c) {}
    virtual ~Node() = default;
    virtual void dump(int d) const = 0;
    virtual llvm::Value* accept(Visitor& v) = 0;
protected:
    static void indent(int d)
    {
        std::cout << string(d * 2, ' ');
    }
};

struct StmtNode : Node { using Node::Node; };
struct ExprNode : Node { using Node::Node; };
struct ValueNode : ExprNode { using ExprNode::ExprNode; };

struct NumberNode : ValueNode {
    int value;
    NumberNode(int l, int c, int v) : ValueNode(l, c), value(v) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Const " << value << "\n";
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_number(*this);
    }
};

struct VarNameNode : ValueNode {
    string name;
    VarNameNode(int l, int c, string n) : ValueNode(l, c), name(std::move(n)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Var " << name << "\n";
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_var(*this);
    }
};
struct BinaryOpNode : ExprNode {
    char op; unique_ptr<ExprNode> left, right;
    BinaryOpNode(int l, int c, char o, unique_ptr<ExprNode> lhs, unique_ptr<ExprNode> rhs)
        : ExprNode(l, c), op(o), left(std::move(lhs)), right(std::move(rhs)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "BinOp " << op << "\n";
        left->dump(d + 1); right->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_binop(*this);
    }
};
struct DeclNode : StmtNode {
    string name; bool is_mut; unique_ptr<ExprNode> expr;
    DeclNode(int l, int c, string n, bool m, unique_ptr<ExprNode> e)
        : StmtNode(l, c), name(std::move(n)), is_mut(m), expr(std::move(e)) {}

    void dump(int d) const override
    {
        indent(d); std::cout << "Decl " << name << (is_mut ? " mut" : " const") << "\n";
        expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_decl(*this);
    }
};
struct AsmtNode : StmtNode {
    string name; unique_ptr<ExprNode> expr;
    AsmtNode(int l, int c, string n, unique_ptr<ExprNode> e)
        : StmtNode(l, c), name(std::move(n)), expr(std::move(e)) {}
    void dump(int d) const override
    {
        indent(d); std::cout << "Assign " << name << "\n";
        expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_asmt(*this);
    }
};
struct ExitNode : Node {
    unique_ptr<ExprNode> expr;
    ExitNode(int l, int c, unique_ptr<ExprNode> e) : Node(l, c), expr(std::move(e)) {}
    void dump(int d) const override
    {
        indent(d); std::cout << "Exit\n"; expr->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_exit(*this);
    }
};
struct ProgramNode : Node {
    vector<unique_ptr<StmtNode>> stmts; unique_ptr<ExitNode> exit_stmt;
    ProgramNode(int l, int c, vector<unique_ptr<StmtNode>> s, unique_ptr<ExitNode> e)
        :Node(l, c), stmts(std::move(s)), exit_stmt(std::move(e)) {}
    void dump(int d) const override {
        indent(d); std::cout << "Program\n";
        for (auto& s : stmts)
        {
            s->dump(d + 1);
        }
        exit_stmt->dump(d + 1);
    }

    llvm::Value* accept(Visitor& v) override
    {
        return v.visit_program(*this);
    }
};

struct CodeGen : Visitor {
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    std::map<std::string, VariableInfo> symbols;
    llvm::Type* i32_type;
    llvm::FunctionCallee printf_func;
    llvm::Constant* fmt_str;

    CodeGen() : builder(context) {
        module = std::make_unique<llvm::Module>("practice2", context);
        module->setTargetTriple(llvm::sys::getDefaultTargetTriple());

        i32_type = llvm::Type::getInt32Ty(context);
    }

    [[noreturn]] void fail(const Node& n, const std::string& msg) {
        throw std::runtime_error("line " + std::to_string(n.line) + ":" + std::to_string(n.col) + ": " + msg);
    }

    llvm::Value* visit_program(ProgramNode& n) override {
        llvm::FunctionType *main_func_type = llvm::FunctionType::get(i32_type, false);
        llvm::Function *main_func = llvm::Function::Create(main_func_type, llvm::Function::ExternalLinkage, "main", module.get());
        llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(context, "entry", main_func);
        builder.SetInsertPoint(entry_bb);
        fmt_str = builder.CreateGlobalStringPtr("Program exit with result %d\n", "fmt");

        llvm::FunctionType *printf_type = llvm::FunctionType::get(i32_type, {llvm::PointerType::getUnqual(context)}, true /* var_arg */);
        printf_func = module->getOrInsertFunction("printf", printf_type);

        for (auto& s : n.stmts)
        {
            s->accept(*this);
        }
        n.exit_stmt->accept(*this);

        return nullptr;
    }

    llvm::Value* visit_decl(DeclNode& n) override {
        if (symbols.count(n.name)) {
            fail(n, "variable '" + n.name + "' is already defined");
        }

        llvm::Value* val = n.expr->accept(*this);

        llvm::AllocaInst* alloca = builder.CreateAlloca(i32_type, nullptr, n.name);

        builder.CreateStore(val, alloca);

        symbols[n.name] = VariableInfo{alloca, n.is_mut};

        return nullptr;
    }

    llvm::Value* visit_asmt(AsmtNode& n) override {
        if (!symbols.count(n.name)) {
            fail(n, "unknown variable '" + n.name + "'");
        }
        if (!symbols[n.name].is_mut) {
            fail(n, "cannot assign to a const variable '" + n.name + "'");
        }

        llvm::Value* val = n.expr->accept(*this);

        builder.CreateStore(val, symbols[n.name].alloca);

        return nullptr;
    }

    llvm::Value* visit_exit(ExitNode& n) override {

        llvm::Value* val = n.expr->accept(*this);

        builder.CreateCall(printf_func, {fmt_str, val});
        builder.CreateRet(val);

        return nullptr;
    }

    llvm::Value* visit_number(NumberNode& n) override {
        return llvm::ConstantInt::get(i32_type, n.value);
    }

    llvm::Value* visit_var(VarNameNode& n) override {
        if (!symbols.count(n.name)) {
            fail(n, "unknown variable '" + n.name + "'");
        }

        return builder.CreateLoad(i32_type, symbols[n.name].alloca, n.name);
    }

    llvm::Value* visit_binop(BinaryOpNode& n) override {
        llvm::Value* l = n.left->accept(*this);
        llvm::Value* r = n.right->accept(*this);

        switch (n.op) {
            case '+': return builder.CreateAdd(l, r, "addtmp");
            case '-': return builder.CreateSub(l, r, "subtmp");
            case '*': return builder.CreateMul(l, r, "multmp");
            default:  fail(n, std::string("unknown binary operator '") + n.op + "'");
        }
    }
};

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
        if (at("keyword", "i32")) return parse_decl();
        if (at("identifier")) return parse_assign();

        throw error_at(*peek(), "cannot start a statement with '" + peek()->text + "'");
    }

    // declaration ::= "i32" [ "mut" ] identifier "{" expression "}"
    unique_ptr<StmtNode> parse_decl() {
        eat();

        bool is_mut = at("keyword", "mut");
        if (is_mut) eat();

        Token name = expect("identifier", "", "a variable name");

        if (!at("block", "{")) {
            throw std::runtime_error(where(name.line, name.col + (int)name.text.size()) + "variable '" + name.text + "' needs an initialiser in {}");
        }

        eat();

        auto init = parse_expr();
        expect("block", "}", "'}'");

        return make_unique<DeclNode>(name.line, name.col, name.text, is_mut, std::move(init));
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

    // expression ::= term { ( "+" | "-" ) term }
    unique_ptr<ExprNode> parse_expr()
    {
        unique_ptr<ExprNode> node = parse_term();

        while (at("operator", "+") || at("operator", "-"))
        {
            Token op = eat();
            auto right = parse_term();
            node = make_unique<BinaryOpNode>(op.line, op.col, op.text[0], std::move(node), std::move(right));
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
            node = make_unique<BinaryOpNode>(op.line, op.col, op.text[0], std::move(node), std::move(right));
        }

        return node;
    }

    // factor ::= number | identifier
    unique_ptr<ExprNode> parse_factor() {

        if (at("identifier")) {
            Token t = eat();
            return make_unique<VarNameNode>(t.line, t.col, t.text);
        }

        if (at("number")) {
            Token t = eat();
            return make_unique<NumberNode>(t.line, t.col, std::stoi(t.text));
        }

        throw error("expected a constant or a variable");
    }
};

int main(int argc, char* argv[])
{
    bool print_ast = false;
    bool print_tokens = false;
    std::vector<std::string> args;

    // command line args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--ast")
        {
            print_ast = true;
        }
        else if (arg == "--tokens")
        {
            print_tokens = true;
        }
        else
        {
            args.push_back(arg);
        }
    }

    if (args.empty()) {
        std::cerr << "Expected at least an input file\n";
        return 1;
    }

    std::string input_path = args[0];

    // read input file
    std::ifstream input_file(input_path);
    if (!input_file.is_open()) {
        std::cerr << "Failed to open input file.\n";
        return 1;
    }

    std::string source_code((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
    input_file.close();

    if (source_code.empty()) {
        std::cerr << "compilation error: line 1: no exit\n";
        return 1;
    }

    try {
        // lexer
        std::vector<std::vector<Token>> token_lines = lex(source_code);

        if (print_tokens) {
            for (const auto& line : token_lines) {
                for (const auto& t : line)
                {
                    std::cout << t.kind << "('" << t.text << "') ";
                }
                std::cout << "\n";
            }
            if (!print_ast) return 0;
        }

        // parsing
        Parser parser(token_lines);
        auto program = parser.parse_program();

        if (print_ast) {
            program->dump(0);
            return 0;
        }

        // llvm
        if (args.size() < 2) {
            std::cerr << "Expected an output file argument for compilation\n";
            return 1;
        }

        std::string output_path = args[1];
        CodeGen cg;
        program->accept(cg);

        std::error_code error_info;
        llvm::raw_fd_ostream output_file(output_path, error_info, llvm::sys::fs::OF_None);

        if (error_info) {
            std::cerr << "Error opening output file: " << error_info.message() << "\n";
            return 1;
        }

        cg.module->print(output_file, nullptr);
        output_file.flush();
        output_file.close();

    }
    catch (const std::exception& e)
    {
        std::cerr << "compilation error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
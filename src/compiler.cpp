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

int main(int argc, char* argv[])
{
    if (argc != 3) {
        std::cerr << "Expected 2 arguments: input and output file\n";
        exit(1);
    }

    std::string input_path(argv[1]);
    std::string output_path(argv[2]);

    std::ifstream input_file(input_path);
    if (!input_file.is_open())
    {
        std::cerr << "Failed to open input file.\n";
        exit(1);
    }

    std::string source_code((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
    input_file.close();

    if (source_code.empty())
    {
        std::cerr << "compilation error: line 1: no exit\n";
        exit(1);
    }

    std::vector<std::vector<Token>> token_lines;
    try {
        token_lines = lex(source_code);
    }
    catch (const std::exception& e)
    {
        std::cerr << "compilation error: " << e.what() << "\n";
        exit(1);
    }

    //LLVM setup
    llvm::LLVMContext context;
    auto the_module = std::make_unique<llvm::Module>("practice2", context);
    the_module->setTargetTriple(llvm::sys::getDefaultTargetTriple());
    llvm::IRBuilder<> builder(context);

    llvm::Type *i32_type = llvm::Type::getInt32Ty(context);

    llvm::FunctionType *main_func_type = llvm::FunctionType::get(i32_type, false);
    llvm::Function *main_func = llvm::Function::Create(main_func_type, llvm::Function::ExternalLinkage, "main", the_module.get());
    llvm::BasicBlock *entry_bb = llvm::BasicBlock::Create(context, "entry", main_func);
    builder.SetInsertPoint(entry_bb);

    llvm::FunctionType *printf_type = llvm::FunctionType::get(i32_type, {llvm::PointerType::getUnqual(context)}, true /* var_arg */);
    llvm::FunctionCallee printf_func = the_module->getOrInsertFunction("printf", printf_type);

    llvm::Constant *fmt_str = builder.CreateGlobalStringPtr("Program exit with result %d\n", "fmt");

    std::map<std::string, VariableInfo> symbols;

    auto fail = [&](int l, int c, const std::string& msg) -> void {
        std::cerr << "compilation error: line " << l << ":" << c << ": " << msg << "\n";
        exit(1);
    };

    auto resolve_value = [&](const Token& token) -> llvm::Value* {
        if (token.kind == "number")
        {
            return llvm::ConstantInt::get(i32_type, std::stoi(token.text), true);
        }
        else if (token.kind == "identifier")
        {
            auto it = symbols.find(token.text);
            if (it == symbols.end()) {
                fail(token.line, token.col, "variable '" + token.text + "' is used before its declaration");
            }
            return builder.CreateLoad(i32_type, it->second.alloca, token.text + "_val");
        }
        else {
            fail(token.line, token.col, "expected a number or a variable, got '" + token.text + "'");
        }
        return nullptr;
    };

    auto resolve_expr = [&](const std::vector<Token>& toks) -> llvm::Value*
    {
        if (toks.empty())
        {
            fail(toks.empty() ? 0 : toks[0].line, toks.empty() ? 0 : toks[0].col, "expected a value");
        }
        if (toks.size() == 1)
        {
            return resolve_value(toks[0]);
        }
        if (toks.size() == 3 && toks[1].kind == "operator" && toks[1].text != ":=")
        {

            llvm::Value *lhs = resolve_value(toks[0]);
            llvm::Value *rhs = resolve_value(toks[2]);
            const std::string &op = toks[1].text;

            if (op == "+") return builder.CreateAdd(lhs, rhs, "addtmp");
            if (op == "-") return builder.CreateSub(lhs, rhs, "subtmp");
            if (op == "*") return builder.CreateMul(lhs, rhs, "multmp");

            fail(toks[1].line, toks[1].col, "unknown operator '" + op + "'");
        }
        fail(toks[0].line, toks[0].col, "expected a value or a single operation (+, -, *) on two values");
        return nullptr;
    };

    bool has_exit = false;
    int last_seen_line = 1;

    for (size_t i = 0; i < token_lines.size(); ++i)
    {
        const std::vector<Token>& current_line = token_lines[i];
        if (current_line.empty()) continue;

        last_seen_line = current_line.front().line;

        //exit
        if (current_line[0].kind == "keyword" && current_line[0].text == "exit")
        {
            if (i != token_lines.size() - 1)
            {
                fail(current_line[0].line, current_line[0].col, "exit not on last line of the program");
            }
            if (current_line.size() != 2)
            {
                fail(current_line[0].line, current_line[0].col, "exit takes exactly one value");
            }

            llvm::Value *exit_val = resolve_value(current_line[1]);
            builder.CreateCall(printf_func, {fmt_str, exit_val});
            builder.CreateRet(llvm::ConstantInt::get(i32_type, 0, true));
            has_exit = true;
        }

        //declaration
        else if (current_line[0].kind == "keyword" && current_line[0].text == "i32")
        {
            bool is_mutable = current_line.size() > 1 && current_line[1].text == "mut";
            size_t name_idx = 1 + is_mutable;

            if (current_line.size() <= name_idx)
            {
                fail(current_line[0].line, current_line[0].col, "declaration is missing a variable name");
            }

            const Token &name_token = current_line[name_idx];

            if (name_token.kind == "keyword")
            {
                fail(name_token.line, name_token.col, "'" + name_token.text + "' cannot be used as a variable name (reserved word)");
            }
            if (name_token.kind != "identifier")
            {
                fail(name_token.line, name_token.col, "expected a variable name, got '" + name_token.text + "'");
            }

            std::string var_name = name_token.text;

            size_t brace_open_idx = name_idx + 1;
            size_t brace_close_idx = current_line.size() - 1;
            bool has_open_brace = current_line.size() > brace_open_idx && current_line[brace_open_idx].text == "{";

            if (!has_open_brace) {
                fail(name_token.line, name_token.col + (int)var_name.size(), "variable '" + var_name + "' needs an initialiser in {}");
            }
            if (current_line.back().text != "}")
            {
                fail(current_line.back().line, current_line.back().col, "extra tokens after the initialiser");
            }


            if (brace_close_idx <= brace_open_idx)
            {
                fail(name_token.line, name_token.col + (int)var_name.size(), "variable '" + var_name + "' needs an initialiser in {}");
            }

            if (symbols.find(var_name) != symbols.end())
            {
                fail(name_token.line, name_token.col, "variable '" + var_name + "' is already declared");
            }

            std::vector<Token> init_tokens(current_line.begin() + brace_open_idx + 1, current_line.begin() + brace_close_idx);
            llvm::Value *init_val = resolve_expr(init_tokens);

            llvm::AllocaInst *slot = builder.CreateAlloca(i32_type, nullptr, var_name);
            builder.CreateStore(init_val, slot);
            symbols[var_name] = VariableInfo{slot, is_mutable};
        }

        //assignment
        else if (current_line.size() >= 3 && current_line[0].kind == "identifier" && current_line[1].text == ":=")
        {
            const Token &name_tok = current_line[0];
            std::string target_var = name_tok.text;

            auto it = symbols.find(target_var);
            if (it == symbols.end())
            {
                fail(name_tok.line, name_tok.col, "variable '" + target_var + "' is used before its declaration");
            }
            if (!it->second.is_mut)
            {
                fail(name_tok.line, name_tok.col, "cannot assign to '" + target_var + "': it is not mutable");
            }

            std::vector<Token> rhs_tokens(current_line.begin() + 2, current_line.end());
            llvm::Value *rhs_val = resolve_expr(rhs_tokens);
            builder.CreateStore(rhs_val, it->second.alloca);
        }

        // syntax error
        else
        {
            fail(current_line[0].line, current_line[0].col, "unparsable line");
        }
    }

    if (!has_exit)
    {
        fail(last_seen_line, 1, "no exit");
    }

    // write to file
    std::error_code error_info;
    llvm::raw_fd_ostream output_file(output_path, error_info, llvm::sys::fs::OF_None);

    if (error_info) {
        std::cerr << "Error opening output file: " << error_info.message() << "\n";
        exit(1);
    }

    the_module->print(output_file, nullptr);
    output_file.flush();
    output_file.close();

    return 0;
}
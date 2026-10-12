#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>

#include "ast.h"
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "semantic_checker.h"

int main(int argc, char* argv[])
{
    bool print_ast    = false;
    bool print_tokens = false;
    std::vector<std::string> args;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if      (arg == "--ast")    { print_ast    = true; }
        else if (arg == "--tokens") { print_tokens = true; }
        else                        { args.push_back(arg); }
    }

    if (args.empty()) { std::cerr << "Expected at least an input file\n"; return 1; }

    std::ifstream input_file(args[0]);
    if (!input_file.is_open()) { std::cerr << "Failed to open input file.\n"; return 1; }
    std::string source((std::istreambuf_iterator<char>(input_file)),
                       std::istreambuf_iterator<char>());
    input_file.close();

    try
    {
        auto token_lines = lex(source);

        if (print_tokens)
        {
            for (const auto& line : token_lines)
            {
                for (const auto& t : line)
                    std::cout << t.kind << "('" << t.text << "') ";
                std::cout << "\n";
            }
            if (!print_ast) return 0;
        }

        Parser parser(token_lines);
        auto program = parser.parse_program();

        SemanticChecker checker;
        program->accept(checker);

        if (print_ast) { program->dump(0); return 0; }

        if (args.size() < 2)
        {
            std::cerr << "Expected an output file argument for compilation\n";
            return 1;
        }

        CodeGen cg;
        program->accept(cg);

        if (llvm::verifyModule(*cg.module, &llvm::errs()))
            throw std::runtime_error("internal error: generated IR is invalid");

        std::error_code ec;
        llvm::raw_fd_ostream out(args[1], ec, llvm::sys::fs::OF_None);
        if (ec) { std::cerr << "Error opening output file: " << ec.message() << "\n"; return 1; }

        cg.module->print(out, nullptr);
        out.flush(); out.close();
    }
    catch (const std::exception& e)
    {
        std::cerr << "compilation error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

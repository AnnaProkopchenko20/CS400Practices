#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/raw_ostream.h>
#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"

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

        // semantic pass: types, symbol table, all checks. No IR exists yet.
        SemanticChecker checker;
        program->accept(checker);

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

        // safety net: a module that fails here is a compiler bug, and nothing is written
        if (llvm::verifyModule(*cg.module, &llvm::errs()))
        {
            throw std::runtime_error("internal error: generated IR is invalid");
        }

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

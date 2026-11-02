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
#include "optimizer/pipeline.h"
#include "parser.h"
#include "semantic_checker.h"

// Flags may appear anywhere on the command line:
//   --ast             print the PARSE tree (before the checker) and stop
//   --ast-checked     print the tree after the checker (and after the pass, if requested)
//   --tokens          print tokens
//   --remove-unused   Practice 9: remove unused variables and functions after the checker
//   --keep-calls      with --remove-unused: Task 1 rule, keep every value that has a call
//   --opt-stats       with --remove-unused: print rounds and live-node counts to stderr
int main(int argc, char* argv[])
{
    bool print_ast         = false;
    bool print_ast_checked = false;
    bool print_tokens      = false;
    bool remove_unused_on  = false;
    bool keep_calls        = false;
    bool opt_stats         = false;
    std::vector<std::string> args;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if      (arg == "--ast")            { print_ast         = true; }
        else if (arg == "--ast-checked")    { print_ast_checked = true; }
        else if (arg == "--tokens")         { print_tokens      = true; }
        else if (arg == "--remove-unused")  { remove_unused_on  = true; }
        else if (arg == "--keep-calls")     { keep_calls        = true; }
        else if (arg == "--opt-stats")      { opt_stats         = true; }
        else                                { args.push_back(arg); }
    }

    if (args.empty()) { std::cerr << "Expected at least an input file\n"; return 1; }

    std::ifstream input_file(args[0]);
    if (!input_file.is_open()) { std::cerr << "Failed to open input file.\n"; return 1; }
    std::string source(
        (std::istreambuf_iterator<char>(input_file)),
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
            if (!print_ast && !print_ast_checked) return 0;
        }

        Parser parser(token_lines);
        auto program = parser.parse_program();

        // --ast is the parse tree: print it before the checker rewrites anything.
        if (print_ast && !print_ast_checked) { program->dump(0); return 0; }

        SemanticChecker checker;
        program->accept(checker);

        // Practice 9: the pass runs only on a tree the checker has accepted.
        if (remove_unused_on)
        {
            int live_before = Node::live;
            OptStats st = remove_unused(*program, keep_calls);
            if (opt_stats)
            {
                std::cerr << "[opt] rounds=" << st.rounds
                          << " removed_stmts=" << st.removed_stmts
                          << " removed_functions=" << st.removed_fns
                          << " live_nodes " << live_before << " -> " << Node::live << "\n";
            }
        }

        if (print_ast_checked) { program->dump(0); return 0; }

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

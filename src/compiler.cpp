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
#include <regex>
#include <format>

int main(int argc, char* argv[]) {
    //args
    if (argc != 3)
    {
        std::cerr << "Expected 2 arguments: input and output file\n";
        exit(1);
    }

    std::string input_path(argv[1]);
    std::string output_path(argv[2]);

    //open and read source file
    std::ifstream input_file(input_path);

    if (!input_file.is_open()) {
        std::cerr << "Failed to open input file.\n";
        exit(1);
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input_file, line)) {
        lines.push_back(line);
    }

    input_file.close();


    if (lines.empty()) {
        std::cerr << "line 1: no exit\n";
        exit(1);
    }

    // setup
    llvm::LLVMContext Context;
    auto TheModule = std::make_unique<llvm::Module>("practice1", Context);
    TheModule->setTargetTriple(llvm::sys::getDefaultTargetTriple());
    llvm::IRBuilder<> Builder(Context);

    llvm::Type *i32Type = llvm::Type::getInt32Ty(Context);

    llvm::FunctionType *MainFuncType = llvm::FunctionType::get(i32Type, false);
    llvm::Function *MainFunc = llvm::Function::Create(MainFuncType, llvm::Function::ExternalLinkage, "main", TheModule.get());
    llvm::BasicBlock *EntryBB = llvm::BasicBlock::Create(Context, "entry", MainFunc);
    Builder.SetInsertPoint(EntryBB);

    llvm::FunctionType *PrintfType = llvm::FunctionType::get(i32Type, {llvm::PointerType::getUnqual(Context)}, true /* var_arg */);
    llvm::FunctionCallee PrintfFunc = TheModule->getOrInsertFunction("printf", PrintfType);

    llvm::Constant *FmtStr = Builder.CreateGlobalStringPtr("Program exit with result %d\n", "fmt");
    std::map<std::string, llvm::AllocaInst *> symbols;

    // read the program and compile
    std::string variable = R"([a-zA-Z_]\w*)";
    std::string value = R"(-?\d+)";
    std::string operation = R"([+\-*])";
    std::string variable_or_constant = R"((?:[a-zA-Z_]\w*|-?\d+))";

    std::regex variable_declaration(std::format(R"(^\s*int\s+({0})\s*$)", variable));
    std::regex variable_assignment_1arg(std::format(R"(^({0})\s*:=\s*({1})\s*$)", variable, variable_or_constant));
    std::regex variable_assignment_3arg(std::format(R"(^({0})\s*:=\s*({1})\s*({2})\s*({1})\s*$)", variable, variable_or_constant, operation));
    std::regex program_exit(std::format(R"(^\s*exit\s+({0})\s*$)", variable_or_constant));

    auto resolveValue = [&](const std::string& token, int line_num) -> llvm::Value* {
        if (isdigit(token[0]) || (token.length() > 1 && token[0] == '-' && isdigit(token[1]))) {
            return llvm::ConstantInt::get(i32Type, std::stoi(token), true);
        } else {
            if (symbols.find(token) == symbols.end()) {
                std::cerr << "line " << line_num << ": undeclared variable\n";
                exit(1);
            }
            return Builder.CreateLoad(i32Type, symbols[token], token + "_val");
        }
    };

    // compile
    for (size_t i = 0; i < lines.size(); ++i) {
        int lineNumber = i + 1;
        std::smatch matches;

        if (std::regex_match(lines[i], matches, program_exit)) {
            if (i != lines.size() - 1) {
                std::cerr << "line " << lineNumber << ": exit not on last line of the program\n";
                exit(1);
            }
            llvm::Value *exitVal = resolveValue(matches[1].str(), lineNumber);
            Builder.CreateCall(PrintfFunc, {FmtStr, exitVal});
            Builder.CreateRet(llvm::ConstantInt::get(i32Type, 0, true));
            continue;
        }

        if (std::regex_match(lines[i], matches, variable_declaration)) {
            std::string varName = matches[1].str();
            if (symbols.find(varName) != symbols.end()) {
                std::cerr << "line " << lineNumber << ": redeclared variable\n";
                exit(1);
            }
            if (varName == "int" || varName == "exit")
            {
                std::cerr << "line " << lineNumber << ": variable name uses reserved symbols int or exit\n";
                exit(1);
            }
            symbols[varName] = Builder.CreateAlloca(i32Type, nullptr, varName);
        }
        else if (std::regex_match(lines[i], matches, variable_assignment_1arg)) {
            std::string targetVar = matches[1].str();
            if (symbols.find(targetVar) == symbols.end()) {
                std::cerr << "line " << lineNumber << ": undeclared variable\n";
                exit(1);
            }

            llvm::Value *rhsVal = resolveValue(matches[2].str(), lineNumber);
            Builder.CreateStore(rhsVal, symbols[targetVar]);
        }
        else if (std::regex_match(lines[i], matches, variable_assignment_3arg)) {
            std::string targetVar = matches[1].str();
            if (symbols.find(targetVar) == symbols.end()) {
                std::cerr << "line " << lineNumber << ": undeclared variable\n";
                exit(1);
            }

            llvm::Value *lhsVal = resolveValue(matches[2].str(), lineNumber);
            std::string op = matches[3].str();
            llvm::Value *rhsVal = resolveValue(matches[4].str(), lineNumber);

            llvm::Value *mathResult = nullptr;
            if (op == "+") {
                mathResult = Builder.CreateAdd(lhsVal, rhsVal, "addtmp");
            } else if (op == "-") {
                mathResult = Builder.CreateSub(lhsVal, rhsVal, "subtmp");
            } else if (op == "*") {
                mathResult = Builder.CreateMul(lhsVal, rhsVal, "multmp");
            }

            Builder.CreateStore(mathResult, symbols[targetVar]);
        }
        else {
            std::cerr << "line " << lineNumber << ": unparsable line\n";
            exit(1);
        }
    }

    if (!std::regex_match(lines.back(), program_exit)) {
        std::cerr << "line " << lines.size() << ": no exit\n";
        exit(1);
    }


    // write to file
    std::error_code error_info;
    llvm::raw_fd_ostream output_file(output_path, error_info, llvm::sys::fs::OF_None);

    if (error_info) {
        std::cerr << "Error opening output file: " << error_info.message() << "\n";
        exit(1);
    }

    TheModule->print(output_file, nullptr);
    output_file.flush();
    output_file.close();

    return 0;
}
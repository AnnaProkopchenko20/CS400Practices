#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using std::string; using std::vector; using std::unique_ptr; using std::make_unique;

struct ProgramNode; struct DeclNode;  struct AsmtNode;    struct ExitNode;
struct NumberNode;  struct BoolNode;  struct VarNameNode; struct BinaryOpNode;
struct IfNode;      struct BlockNode; struct NotNode;     struct WhileNode;

struct StructDeclNode;
struct FieldDeclNode;
struct FunctionDeclNode;
struct FuncParamNode;
struct FunctionCallNode;
struct StructFieldAccessNode;
struct InitNode;

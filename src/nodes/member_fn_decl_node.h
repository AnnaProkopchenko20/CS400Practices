#pragma once
#include "fn_decl_node.h"

struct MemberFnDeclNode : FunctionDeclNode {
    std::string name;
    std::string return_type;
    std::vector<std::unique_ptr<FuncParamNode>> params;
    std::unique_ptr<BlockNode> body;
    // weak pointer to structure it is memeber function of
};

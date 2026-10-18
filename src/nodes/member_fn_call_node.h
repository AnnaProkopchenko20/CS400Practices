#pragma once
#include <string>

#include "member_access_node.h"

struct MemberFnCallNode : MemberAccessNode {
    // return type
    std::string fn_name;
    // ExprNode to the left of the access dot
    DeclNode* base_decl = nullptr;
};

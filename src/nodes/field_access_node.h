#pragma once


struct StructFieldAccessNode : MemberAccessNode {
    // return type
    std::string field_name;
    // ExprNode to the left of the access dot
    DeclNode* base_decl = nullptr;

};

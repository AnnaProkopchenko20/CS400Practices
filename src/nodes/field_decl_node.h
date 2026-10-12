#pragma once
#include "stmt_node.h"

struct FieldDeclNode : StmtNode {
    string type_name;   // "i32", "i64", or "bool"
    string name;
    bool   is_mut;
};

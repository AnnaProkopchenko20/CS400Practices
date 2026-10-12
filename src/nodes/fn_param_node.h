#pragma once
#include "stmt_node.h"

struct FuncParamNode : Node {
    string type_name;   // "i32", "i64", or "bool"
    string name;
    bool   is_mut = false;
};

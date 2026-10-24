#pragma once
#include "node.h"

struct ExprNode : Node {
    string type;

    using Node::Node;
};

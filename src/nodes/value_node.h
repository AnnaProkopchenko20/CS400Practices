#pragma once
#include "expr_node.h"

// Base for leaf expression nodes (literals and variable references).
struct ValueNode : ExprNode { using ExprNode::ExprNode; };

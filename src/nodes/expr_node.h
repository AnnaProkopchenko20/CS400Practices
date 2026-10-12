#pragma once
#include "node.h"

// Abstract base for all expression nodes.
// The .type field is filled in by SemanticChecker: "i32", "i64", or "bool".
struct ExprNode : Node { using Node::Node; string type; };

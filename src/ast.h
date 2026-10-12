#pragma once
// Single umbrella include for the whole AST.
// Consumers should include only this file, not individual node headers.
#include "ast_fwd.h"
#include "keywords.h"
#include "visitor.h"

#include "nodes/node.h"
#include "nodes/stmt_node.h"
#include "nodes/expr_node.h"
#include "nodes/value_node.h"

#include "nodes/number_node.h"
#include "nodes/var_name_node.h"
#include "nodes/bool_node.h"
#include "nodes/binary_op_node.h"
#include "nodes/not_node.h"

#include "nodes/decl_node.h"
#include "nodes/asmt_node.h"
#include "nodes/exit_node.h"
#include "nodes/block_node.h"
#include "nodes/if_node.h"
#include "nodes/while_node.h"
#include "nodes/program_node.h"

#pragma once
#include "ast.h"

struct OptStats
{
    int rounds        = 0;
    int removed_stmts = 0;   // declarations and assignments
    int removed_fns   = 0;
};

// Runs the Practice 9 pass on a CHECKED tree until a round removes nothing.
//   conservative == true : a value that contains any call or read() is kept (Task 1 rule)
//   conservative == false: a value is kept only if it contains read() or a call of a
//                          function with an effect (additional task)
OptStats remove_unused(ProgramNode& program, bool conservative);

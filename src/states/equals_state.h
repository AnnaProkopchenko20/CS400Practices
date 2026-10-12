#pragma once
#include "../lexer_context.h"

// After '=' -- only "==" is valid in Practice 5.
// A single '=' is not an operator; "=>" does not exist in this grammar.
class EqualsState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (!eof && b == '=')
        {
            ctx.emit("operator", "==", ctx.line, ctx.startCol);
            ctx.changeState(StateId::Start);
            return true;
        }
        ctx.fail("expected '==' (a single '=' is not an operator)", ctx.line, ctx.startCol);
    }
};

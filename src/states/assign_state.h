#pragma once
#include "../lexer_context.h"

// After ':' -- only ":=" is valid in Practice 5.
class AssignState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (!eof && b == '=')
        {
            ctx.emit("operator", ":=", ctx.line, ctx.startCol);
            ctx.changeState(StateId::Start);
            return true;
        }
        ctx.fail("':' not followed by '='", ctx.line, ctx.startCol);
    }
};

#pragma once
#include "../lexer_context.h"

class NegationState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (!eof && b == '=')
        {
            ctx.emit("operator", "!=", ctx.line, ctx.startCol);
            ctx.changeState(StateId::Start);
            return true;
        }
        ctx.emit("operator", "!", ctx.line, ctx.startCol);
        ctx.changeState(StateId::Start);
        return false;
    }
};

#pragma once
#include "../lexer_context.h"

class MinusState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (!eof && b == '>')
        {
            ctx.emit("operator", "->", ctx.line, ctx.startCol);
            ctx.changeState(StateId::Start);
            return true;
        }
        else
        {
            ctx.emit("operator", "-", ctx.line, ctx.startCol);
            ctx.changeState(StateId::Start);
            return false;
        }
    }
};

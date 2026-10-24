#pragma once
#include "../lexer_context.h"

class StringState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (eof || b == '\n')
        {
            ctx.fail("string literal is not closed on this line", ctx.line, ctx.startCol);
        }
        if (b == '"')
        {
            std::string text = ctx.data.substr(ctx.start, ctx.i - ctx.start + 1);
            ctx.emit("string", text, ctx.line, ctx.startCol);
            ctx.changeState(StateId::Start);
            return true;
        }
        return true;
    }
};

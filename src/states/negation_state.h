#pragma once
#include "../lexer_context.h"

// After '!' -- "!=" (inequality) or unary "!" (logical not).
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
        // Unary '!': emit and re-handle the current byte in StartState
        ctx.emit("operator", "!", ctx.line, ctx.startCol);
        ctx.changeState(StateId::Start);
        return false;
    }
};

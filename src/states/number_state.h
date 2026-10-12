#pragma once
#include "../lexer_context.h"

// Decimal integer literal.
class NumberState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (!eof && lexchar::isDigit(b)) return true;
        if (!eof && (lexchar::isAsciiLetter(b) || b == '_' || b >= 0x80))
            ctx.fail("unexpected letter inside a number", ctx.line, ctx.col);
        ctx.emitLexeme("number");
        ctx.changeState(StateId::Start);
        return false;
    }
};

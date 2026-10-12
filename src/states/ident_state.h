#pragma once
#include "../lexer_context.h"

// ASCII identifier or keyword.
// Any byte >= 0x80 is rejected because Practice 5 identifiers are ASCII-only.
class IdentState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (eof) return finishWord(ctx);
        if (lexchar::isAsciiLetter(b) || lexchar::isDigit(b) || b == '_')
            return true;
        if (b > 127)
            ctx.fail("unexpected byte", ctx.line, ctx.col);
        return finishWord(ctx);
    }

private:
    bool finishWord(LexerContext& ctx)
    {
        ctx.emitLexeme(ctx.classify(ctx.lexeme()));
        ctx.changeState(StateId::Start);
        return false;
    }
};

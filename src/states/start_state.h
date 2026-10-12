#pragma once
#include "../lexer_context.h"

// Between tokens: skips whitespace, emits single-byte tokens, transitions to
// a new state for multi-byte tokens.
// Practice 5 is ASCII-only; any byte >= 0x80 is immediately rejected.
class StartState : public LexerState
{
public:
    bool handle(LexerContext& ctx, bool eof, unsigned char b) override
    {
        if (eof) { ctx.finished = true; return true; }

        if (b == ' ' || b == '\t' || b == '\r') return true;

        if (b == '\n') { ctx.endLine(); return true; }

        if (lexchar::isAsciiLetter(b) || b == '_')
        {
            ctx.markStart();
            ctx.changeState(StateId::Ident);
            return false;
        }
        if (lexchar::isDigit(b))
        {
            ctx.markStart();
            ctx.changeState(StateId::Number);
            return false;
        }

        if (b == '{' || b == '}')
        {
            ctx.emit("block", std::string(1, (char)b), ctx.line, ctx.col);
            return true;
        }

        if (b == '(' || b == ')')
        {
            ctx.emit("parentheses", std::string(1, (char)b), ctx.line, ctx.col);
            return true;
        }


        if (b == ',') { ctx.emit("comma", ",", ctx.line, ctx.col); return true; }
        if (b == '.') { ctx.emit("dot",   ".", ctx.line, ctx.col); return true; }

        if (b == '+' || b == '*')
        {
            ctx.emit("operator", std::string(1, (char)b), ctx.line, ctx.col);
            return true;
        }


        if (b == ':') { ctx.markStart(); ctx.changeState(StateId::Assign);   return true; }
        if (b == '-') { ctx.markStart(); ctx.changeState(StateId::MinusState); return true; }
        if (b == '=') { ctx.markStart(); ctx.changeState(StateId::Equals);   return true; }
        if (b == '!') { ctx.markStart(); ctx.changeState(StateId::Negation); return true; }


        if (b > 127)
            ctx.fail("unexpected byte", ctx.line, ctx.col);
        ctx.fail("unexpected byte " + LexerContext::describeByte(b), ctx.line, ctx.col);
    }
};

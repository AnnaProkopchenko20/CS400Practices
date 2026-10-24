#include "lexer.h"
#include "lexer_context.h"
#include "states/lexer_states.h"

// State-machine driver.  Each byte is fed to the current state's handle()
// method.  States return true if the byte was consumed (advance) or false if
// it should be re-processed by the new state (no advance).
std::vector<std::vector<Token>> lex(const std::string& data)
{
    LexerContext ctx(data);

    while (!ctx.finished)
    {
        bool eof = (ctx.i >= data.size());
        unsigned char b = eof ? 0 : (unsigned char)data[ctx.i];

        bool consumed = ctx.state->handle(ctx, eof, b);
        ctx.applyPendingState();

        if (consumed)
        {
            if (eof) { ctx.finished = true; }
            else     { ctx.advance(); }
        }
    }

    return ctx.takeLines();
}

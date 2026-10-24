#pragma once
#include <memory>

class LexerContext;

enum class StateId { Start, Ident, Number, Assign, Equals, Negation, MinusState };

// Abstract base of all lexer states (State pattern).
//
// handle() processes one byte:
//   returns true  -> byte consumed; advance to the next one.
//   returns false -> byte NOT consumed; re-handle in the new state.
class LexerState
{
public:
    virtual ~LexerState() = default;
    virtual bool handle(LexerContext& ctx, bool eof, unsigned char b) = 0;
};

// Factory: creates a concrete state by id.
// Declared here; DEFINED in states/lexer_states.h (after all concrete states
// are visible) so that concrete state headers can include lexer_context.h
// without a circular dependency.
inline std::unique_ptr<LexerState> makeState(StateId id);

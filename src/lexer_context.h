#pragma once
#include <cstdio>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "lexer.h"
#include "states/lexer_state.h"
#include "keywords.h"

// Byte-classification helpers.
// Practice 5 is an ASCII-only language: identifiers may contain ASCII letters,
// digits and underscores.  Any byte >= 0x80 is illegal.
namespace lexchar
{
    inline bool isAsciiLetter(unsigned char b)
    {
        return (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z');
    }
    inline bool isDigit(unsigned char b) { return b >= '0' && b <= '9'; }
}

// Context of the State pattern: it owns the current state, the input position
// and the accumulated tokens.  States communicate with the lexer only through
// this class.
class LexerContext
{
public:
    const std::string& data;
    size_t i        = 0;
    int    line     = 1;
    int    col      = 1;
    size_t start    = 0;       // byte offset where the current lexeme began
    int    startCol = 1;       // column where the current lexeme began
    bool   finished = false;
    std::unique_ptr<LexerState> state;

    explicit LexerContext(const std::string& src)
        : data(src), state(makeState(StateId::Start)),
          keywords(kw::keyword_table()) {}

    // ---- state switching (applied after handle() returns, never mid-call) ----

    void changeState(StateId id) { pending = id; hasPending = true; }

    void applyPendingState()
    {
        if (hasPending)
        {
            hasPending = false;
            state = makeState(pending);
        }
    }

    // ---- lexeme helpers ----

    void markStart() { start = i; startCol = col; }

    std::string lexeme() const { return data.substr(start, i - start); }

    std::string classify(const std::string& word) const
    {
        auto it = keywords.find(word);
        return it != keywords.end() ? it->second : "identifier";
    }

    void emit(const std::string& kind, const std::string& text, int l, int c)
    {
        tokens.push_back({kind, text, l, c});
    }

    void emitLexeme(const std::string& kind) { emit(kind, lexeme(), line, startCol); }

    // ---- input movement ----

    void endLine()
    {
        lines.push_back(tokens);
        tokens.clear();
        line += 1;
        col = 0;    // advance() will make it 1
    }

    void advance() { i += 1; col += 1; }

    // ---- errors ----

    [[noreturn]] void fail(const std::string& msg, int l, int c) const
    {
        throw std::runtime_error(
            "line " + std::to_string(l) + ":" + std::to_string(c) + ": " + msg);
    }

    // Render a printable ASCII byte as 'x'; others as hex.
    static std::string describeByte(unsigned char b)
    {
        if (b >= 0x20 && b < 0x7F)
            return std::string("'") + (char)b + "'";
        char buf[8];
        std::snprintf(buf, sizeof(buf), "0x%02X", b);
        return buf;
    }

    std::vector<std::vector<Token>> takeLines()
    {
        if (!tokens.empty()) { lines.push_back(tokens); tokens.clear(); }
        return std::move(lines);
    }

private:
    std::map<std::string, std::string> keywords;
    std::vector<Token>                 tokens;
    std::vector<std::vector<Token>>    lines;
    StateId pending    = StateId::Start;
    bool    hasPending = false;
};

#pragma once
#include <string>
#include <vector>

// A single token produced by the lexer.
//   kind : "keyword" | "identifier" | "number" | "operator" | "block"
//   text : the exact source text
//   line : 1-indexed source line
//   col  : 1-indexed column (counted in characters)
struct Token
{
    std::string kind;
    std::string text;
    int line;
    int col;
};

// Returns tokens grouped by source line (the newline acts as the NL terminal).
// Throws std::runtime_error("line L:C: message") on a lexical error.
std::vector<std::vector<Token>> lex(const std::string& data);

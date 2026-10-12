#pragma once
#include <map>
#include <string>

// The single source of truth for all Practice 5 keyword spellings.
// Every module that needs to compare against or print keywords must use these
// constants instead of scattered string literals.
namespace kw
{
    // Types
    inline constexpr const char* I32    = "i32";
    inline constexpr const char* I64    = "i64";
    inline constexpr const char* BOOL   = "bool";

    // Mutability modifier
    inline constexpr const char* MUT    = "mut";

    // Boolean literals
    inline constexpr const char* TRUE_  = "true";
    inline constexpr const char* FALSE_ = "false";

    // Control flow
    inline constexpr const char* IF     = "if";
    inline constexpr const char* ELSE   = "else";
    inline constexpr const char* WHILE  = "while";
    inline constexpr const char* EXIT   = "exit";

    // Used in DeclNode::dump() for the immutable flag (no keyword in the language)
    inline constexpr const char* IMMUTABLE_FLAG = "const";

    // word -> token kind, used by the lexer to classify identifiers
    inline std::map<std::string, std::string> keyword_table()
    {
        return {
            {I32,    "keyword"}, {I64,    "keyword"}, {BOOL,   "keyword"},
            {MUT,    "keyword"}, {TRUE_,  "keyword"}, {FALSE_, "keyword"},
            {IF,     "keyword"}, {ELSE,   "keyword"}, {WHILE,  "keyword"},
            {EXIT,   "keyword"}
        };
    }

    // Returns the canonical keyword spelling for a type name.
    // In Practice 5 all keywords are plain ASCII, so this is the identity function.
    inline const std::string& type_keyword(const std::string& t) { return t; }
}

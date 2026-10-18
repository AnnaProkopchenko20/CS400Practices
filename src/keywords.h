#pragma once
#include <map>
#include <string>

namespace kw
{
    inline constexpr const char* I32    = "i32";
    inline constexpr const char* I64    = "i64";
    inline constexpr const char* BOOL   = "bool";

    inline constexpr const char* FN     = "fn";
    inline constexpr const char* STRUCT = "struct";

    inline constexpr const char* MUT    = "mut";

    inline constexpr const char* TRUE_  = "true";
    inline constexpr const char* FALSE_ = "false";

    inline constexpr const char* IF     = "if";
    inline constexpr const char* ELSE   = "else";
    inline constexpr const char* WHILE  = "while";
    inline constexpr const char* EXIT   = "exit";

    inline constexpr const char* READ   = "read";
    inline constexpr const char* PRINT   = "print";

    inline constexpr const char* IMMUTABLE_FLAG = "const";

    inline std::map<std::string, std::string> keyword_table()
    {
        return {
            {I32,    "keyword"}, {I64,    "keyword"}, {BOOL,   "keyword"},
            {MUT,    "keyword"}, {TRUE_,  "keyword"}, {FALSE_, "keyword"},
            {IF,     "keyword"}, {ELSE,   "keyword"}, {WHILE,  "keyword"},
            {EXIT,   "keyword"}, {FN,     "keyword"}, {STRUCT, "keyword"},
            {READ,   "keyword"}, {PRINT,     "keyword"}
        };
    }

    inline const std::string& type_keyword(const std::string& t) { return t; }
}

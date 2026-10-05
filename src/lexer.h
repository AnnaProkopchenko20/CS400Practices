#pragma once
#include <string>
#include <vector>

struct Token
{
    std::string kind;
    std::string text;
    int line;
    int col;
};

std::vector<std::vector<Token>> lex(const std::string& data);

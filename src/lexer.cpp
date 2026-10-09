#include "lexer.h"
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<std::vector<Token>> lex(const std::string& data)
{
    std::vector<std::vector<Token>> lines;
    std::vector<Token> tokens;

    enum class State { START, IDENT, NUMBER, ASSIGNMENT, EQUALITY, NEGATION};
    State state = State::START;

    int start = 0, line = 1, col = 1;
    size_t i = 0;

    std::map<std::string, std::string> keywords =
    {
        {"i32", "keyword"},
        {"mut", "keyword"},
        {"exit", "keyword"},
        {"i64", "keyword"},
        {"bool", "keyword"},
        {"true", "keyword"},
        {"false", "keyword"},
        {"if", "keyword"},
        {"else", "keyword"},
        {"while", "keyword"}
    };

    while (i <= data.length())
    {
        bool is_eof = (i == data.length());
        unsigned char b = is_eof ? 0 : data[i];

        if (!is_eof && b > 127)
        {
            throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col) + ": unexpected byte");
        }

        if (state == State::START)
        {
            if (is_eof) { break; }

            else if (b == ' ' || b == '\t' || b == '\r')
            {
                // pass
            }
            else if (std::isalpha(b) || b == '_')
            {
                state = State::IDENT;
                start = i;
            }
            else if (b == '\n')
            {
                lines.push_back(tokens);
                tokens.clear();
                line += 1;
                col = 0;
            }
            else if (std::isdigit(b))
            {
                state = State::NUMBER;
                start = i;
            }
            else if (b == '{')
            {
                tokens.push_back({"block", "{", line, col});
            }
            else if (b == '}')
            {
                tokens.push_back({"block", "}", line, col});
            }
            else if (b == '+' || b == '-' || b == '*')
            {
                tokens.push_back({"operator", std::string(1, b), line, col});
            }
            else if (b == ':')
            {
                state = State::ASSIGNMENT;
                start = i;
            }
            else if (b == '=')
            {
                state = State::EQUALITY;
                start = i;
            }
            else if (b == '!')
            {
                state = State::NEGATION;
                start = i;
            }
            else
            {
                throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col) + ": unexpected byte '" + std::string(1, b) + "'");
            }
        }
        else if (state == State::IDENT)
        {
            if (!is_eof && (std::isalpha(b) || std::isdigit(b) || b == '_'))
            {
                // pass
            } else
            {
                std::string word = data.substr(start, i - start);
                std::string kind = keywords.count(word) ? keywords[word] : "identifier";
                tokens.push_back({kind, word, line, col - (int)(i - start)});
                state = State::START;
                continue;
            }
        }
        else if (state == State::NUMBER)
        {
            if (!is_eof && std::isdigit(b))
            {
                // pass
            }
            else if (!is_eof && std::isalpha(b))
            {
                throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col) + ": unexpected letter inside a number");
            }
            else {
                std::string word = data.substr(start, i - start);
                tokens.push_back({"number", word, line, col - (int)(i - start)});
                state = State::START;
                continue;
            }
        }
        else if (state == State::ASSIGNMENT)
        {
            if (b == '=')
            {
                tokens.push_back({"operator", ":=", line, col - 1});
                state = State::START;
            }
            else
            {
                throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col - 1) + ": ':' not followed by '='");
            }
        }
        else if (state == State::EQUALITY)
        {
            if (b == '=')
            {
                tokens.push_back({"operator", "==", line, col - 1});
                state = State::START;
            }
            else
            {
                throw std::runtime_error("line " + std::to_string(line) + ":" + std::to_string(col - 1) + ": expected '==' (a single '=' is not an operator)");
            }
        }
        else if (state == State::NEGATION)
        {
            if (b == '=')
            {
                tokens.push_back({"operator", "!=", line, col - 1});
                state = State::START;
            }
            else
            {
                tokens.push_back({"operator", "!", line, col - 1});
                state = State::START;
                continue;
            }
        }

        i += 1;
        col += 1;
    }

    if (!tokens.empty())
    {
        lines.push_back(tokens);
    }

    return lines;
}

#include "lexer.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <optional>
#include <algorithm>
#include <cctype>

static bool isComment(const std::string &contents, int i)
{
    return i + 1 < contents.size() && contents[i] == '/' && contents[i + 1] == '/';
}

static bool isWhitespace(char c)
{
    return std::isspace(static_cast<unsigned char>(c));
}

static const std::unordered_map<char, TokenType> symbolTable = {
    {';', TokenType::Semicolon},
    {'=', TokenType::Assign},
    {',', TokenType::Comma},
    {'{', TokenType::LBrace},
    {'}', TokenType::RBrace},
    {'(', TokenType::LParen},
    {')', TokenType::RParen},
    {'[', TokenType::LSquareBrace},
    {']', TokenType::RSquareBrace},
    {'+', TokenType::Add},
    {'-', TokenType::Sub},
    {'*', TokenType::Mul},
    {'/', TokenType::Div},
    {'&', TokenType::And},
    {'|', TokenType::Or},
    {'!', TokenType::Not},
    {'.', TokenType::Dot},
    {':', TokenType::Colon}};

std::optional<Token> Lexer::readSymbol(const std::string &contents, int &i, int lineNum)
{
    auto it = symbolTable.find(contents[i]);
    if (it == symbolTable.end())
        return std::nullopt;
    Token tok{it->second, std::string(1, contents[i]), lineNum};
    i++;
    return tok;
}

std::optional<Token> Lexer::readNumber(const std::string &contents, int &i, int lineNum)
{
    if (!std::isdigit(contents[i]))
        return std::nullopt;

    int start = i;

    // Handle the case where number starts with 0
    if (contents[i] == '0')
    {
        i++;
        // If next character is a digit, this is invalid (leading zero)
        if (i < contents.size() && std::isdigit(contents[i]))
        {
            // Consume the rest of the invalid number
            while (i < contents.size() && std::isdigit(contents[i]))
            {
                i++;
            }
            return Token{TokenType::Error, "invalid number with leading zero", lineNum};
        }
        // Single '0' is valid
        return Token{TokenType::IntLiteral, "0", lineNum};
    }

    // Regular number - consume all digits
    while (i < contents.size() && std::isdigit(contents[i]))
    {
        i++;
    }

    // Check if number is followed by invalid characters (like letters)
    if (i < contents.size() && (std::isalpha(contents[i]) || contents[i] == '_'))
    {
        // This is an invalid token like "123abc" or "456_"
        int errorStart = start;
        while (i < contents.size() && (std::isalnum(contents[i]) || contents[i] == '_'))
        {
            i++;
        }
        return Token{TokenType::Error,
                     "invalid token '" + contents.substr(errorStart, i - errorStart) + "'", lineNum};
    }

    return Token{TokenType::IntLiteral, contents.substr(start, i - start), lineNum};
}

static bool isValidChar(char c)
{
    // ASCII values between 32 and 126 inclusive, excluding quote (") and backslash (\)
    return (32 <= c && c <= 126 && c != '"' && c != '\\');
}

std::optional<Token> Lexer::readString(const std::string &contents, int &i, int lineNum)
{
    if (contents[i] != '"')
        return std::nullopt;

    int start = i;
    i++;                       // skip opening quote
    std::string result = "\""; // include opening quote in result
    bool hasError = false;
    std::string errorMsg = "";

    while (i < contents.size())
    {
        char c = contents[i];

        // Check for closing quote
        if (c == '"')
        {
            result += '"'; // include closing quote
            i++;
            if (hasError)
            {
                return Token{TokenType::Error, errorMsg, lineNum};
            }
            return Token{TokenType::StringLiteral, result, lineNum};
        }

        // Handle escape sequences
        if (c == '\\')
        {
            if (i + 1 >= contents.size())
            {
                return Token{TokenType::Error, "unterminated escape sequence", lineNum};
            }
            char next = contents[i + 1];
            switch (next)
            {
            case '"':
                result += "\\\"";
                i += 2;
                break;
            case '\\':
                result += "\\\\";
                i += 2;
                break;
            case 'n':
                result += "\\n";
                i += 2;
                break;
            case 't':
                result += "\\t";
                i += 2;
                break;
            default:
                // Invalid escape sequence
                if (!hasError)
                {
                    hasError = true;
                    errorMsg = std::string("invalid escape sequence \\") + next;
                }
                result += "\\";
                result += next;
                i += 2;
                break;
            }
        }
        // Handle regular characters
        else if (isValidChar(c))
        {
            result.push_back(c);
            i++;
        }
        // Handle invalid characters in string
        else
        {
            if (!hasError)
            {
                hasError = true;
                if (c < 32 || c > 126)
                {
                    errorMsg = "invalid character in string (ASCII " + std::to_string(static_cast<int>(c)) + ")";
                }
                else
                {
                    errorMsg = std::string("invalid character in string: '") + c + "'";
                }
            }
            // Continue parsing but mark as error
            result.push_back(c);
            i++;
        }
    }

    // Reached end of line without closing quote
    return Token{TokenType::Error, "unterminated string literal", lineNum};
}

static const std::unordered_map<std::string, TokenType> keywordTable = {
    {"global", TokenType::Keyword},
    {"return", TokenType::Keyword},
    {"while", TokenType::Keyword},
    {"if", TokenType::Keyword},
    {"else", TokenType::Keyword},
    {"fun", TokenType::Keyword},
    {"None", TokenType::Keyword},
    {"true", TokenType::BooleanLiteral},
    {"false", TokenType::BooleanLiteral}};

std::optional<Token> Lexer::readIdentifierOrKeyword(const std::string &contents, int &i, int lineNum)
{
    if (!std::isalpha(contents[i]) && contents[i] != '_')
        return std::nullopt;

    int start = i;

    // Consume first character (letter or underscore)
    i++;

    // Consume remaining alphanumeric characters and underscores
    while (i < contents.size() && (std::isalnum(contents[i]) || contents[i] == '_'))
        i++;

    std::string word = contents.substr(start, i - start);

    // Check for keywords and boolean literals
    auto it = keywordTable.find(word);
    if (it != keywordTable.end())
        return Token{it->second, word, lineNum};

    return Token{TokenType::Identifier, word, lineNum};
}

std::optional<Token> Lexer::readComparison(const std::string &contents, int &i, int lineNum)
{
    if (i + 1 < contents.size())
    {
        std::string two = contents.substr(i, 2);
        if (two == "<=")
        {
            i += 2;
            return Token{TokenType::Leq, two, lineNum};
        }
        else if (two == ">=")
        {
            i += 2;
            return Token{TokenType::Geq, two, lineNum};
        }
        else if (two == "==")
        {
            i += 2;
            return Token{TokenType::Eq, two, lineNum};
        }
    }
    if (i < contents.size())
    {
        if (contents[i] == '<')
        {
            return Token{TokenType::Lt, std::string(1, contents[i++]), lineNum};
        }
        if (contents[i] == '>')
        {
            return Token{TokenType::Gt, std::string(1, contents[i++]), lineNum};
        }
    }

    return std::nullopt;
}

Lexer::Lexer(const std::string &input) : input_(input) {}

std::vector<Token> Lexer::lex()
{
    std::stringstream lines(input_);
    std::string line;
    int numLines = 0;

    while (std::getline(lines, line))
    {
        numLines++;

        // Process the line character by character
        for (int i = 0; i < line.size();)
        {
            // Skip comments
            if (isComment(line, i))
            {
                break;
            }

            // Skip whitespace
            if (isWhitespace(line[i]))
            {
                i++;
                continue;
            }

            std::optional<Token> t;

            // Try to read different token types in order of precedence

            // 1. String literals (must be before other symbols because of quote)
            if ((t = readString(line, i, numLines)).has_value())
            {
                addToken(t.value());
                continue;
            }

            // 2. Numbers (must be before identifiers to avoid confusion)
            if ((t = readNumber(line, i, numLines)).has_value())
            {
                addToken(t.value());
                continue;
            }

            // 3. Identifiers and keywords
            if ((t = readIdentifierOrKeyword(line, i, numLines)).has_value())
            {
                addToken(t.value());
                continue;
            }

            // 4. Comparison operators (multi-character, before single-character)
            if ((t = readComparison(line, i, numLines)).has_value())
            {
                addToken(t.value());
                continue;
            }

            // 5. Single-character symbols
            if ((t = readSymbol(line, i, numLines)).has_value())
            {
                handleBrackets(t.value());
                addToken(t.value());
                continue;
            }

            // 6. If we get here, we have an unrecognized character sequence
            // This should be rare due to our earlier validation, but handle it
            std::string unrecognizedChar(1, line[i]);
            addToken(Token{TokenType::Error,
                           "unrecognized character '" + unrecognizedChar + "'", numLines});
            i++;
        }
    }

    // Check for unmatched opening brackets at end of input
    while (!stack_.empty())
    {
        Token t = stack_.top();
        addToken(Token{TokenType::Error, "unmatched '" + t.str + "'", t.line});
        stack_.pop();
    }

    addToken(Token{TokenType::EoF, "", numLines});
    return tokens_;
}

void Lexer::printTokens(std::ostream &outstream)
{
    for (const Token &t : tokens_)
    {
        std::string type = "";

        switch (t.type)
        {
        case TokenType::StringLiteral:
            type = " STRINGLITERAL";
            break;
        case TokenType::IntLiteral:
            type = " INTLITERAL";
            break;
        case TokenType::BooleanLiteral:
            type = " BOOLEANLITERAL";
            break;
        case TokenType::Identifier:
            type = " IDENTIFIER";
            break;
        default:
            break;
        }

        if (t.type != TokenType::EoF && t.type != TokenType::Error)
            outstream << t.line << type << " " << t.str << std::endl;
    }
}

void Lexer::printErrors(std::ostream &outstream)
{
    for (const Token &t : tokens_)
    {
        std::string type = "";

        switch (t.type)
        {
        case TokenType::Error:
            type = " ERROR line";
            break;
        case TokenType::StringLiteral:
            type = " STRINGLITERAL";
            break;
        case TokenType::IntLiteral:
            type = " INTLITERAL";
            break;
        case TokenType::BooleanLiteral:
            type = " BOOLEANLITERAL";
            break;
        case TokenType::Identifier:
            type = " IDENTIFIER";
            break;
        default:
            break;
        }

        if (t.type != TokenType::EoF)
            outstream << t.line << type << " " << t.str << std::endl;
    }
}

void Lexer::addToken(const Token &t)
{
    tokens_.push_back(t);
}

void Lexer::handleBrackets(const Token &t)
{
    switch (t.type)
    {
    case TokenType::LBrace:
    case TokenType::LParen:
    case TokenType::LSquareBrace:
        stack_.push(t);
        break;
    case TokenType::RBrace:
        if (!stack_.empty() && stack_.top().type == TokenType::LBrace)
            stack_.pop();
        else
            addToken(Token{TokenType::Error, "unmatched '}'", t.line});
        break;
    case TokenType::RParen:
        if (!stack_.empty() && stack_.top().type == TokenType::LParen)
            stack_.pop();
        else
            addToken(Token{TokenType::Error, "unmatched ')'", t.line});
        break;
    case TokenType::RSquareBrace:
        if (!stack_.empty() && stack_.top().type == TokenType::LSquareBrace)
            stack_.pop();
        else
            addToken(Token{TokenType::Error, "unmatched ']'", t.line});
        break;
    default:
        break;
    }
}

#pragma once

#include <string>
#include <vector>
#include <stack>
#include <optional>
#include <algorithm>

enum class TokenType
{
    Error,
    None,
    Assign,
    LBrace,
    RBrace,
    LParen,
    RParen,
    LSquareBrace,
    RSquareBrace,
    Semicolon,
    Comma,
    Dot,
    Colon,
    Add,
    Sub,
    Mul,
    Div,
    Eq,
    Lt,
    Gt,
    Leq,
    Geq,
    And,
    Or,
    Not,
    IntLiteral,
    StringLiteral,
    BooleanLiteral,
    Keyword,
    Identifier,
    EoF
};

struct Token
{
    TokenType type;
    std::string str;
    int line;
};

class Lexer
{
public:
    explicit Lexer(const std::string &input);

    std::vector<Token> lex();
    void printTokens(std::ostream &outstream);
    void printErrors(std::ostream &outstream);

private:
    std::string input_;
    std::vector<Token> tokens_;
    std::stack<Token> stack_;

    std::optional<Token> readString(const std::string &contents, int &i, int lineNum);
    std::optional<Token> readNumber(const std::string &contents, int &i, int lineNum);
    std::optional<Token> readIdentifierOrKeyword(const std::string &contents, int &i, int lineNum);
    std::optional<Token> readComparison(const std::string &contents, int &i, int lineNum);
    std::optional<Token> readSymbol(const std::string &contents, int &i, int lineNum);
    void addToken(const Token &t);
    void handleBrackets(const Token &t);
};

// parse_location_or_call for those in the same FIRST(<call>) = FIRST(<loc>)
// parse location first
// then
// if peek() is an open paren, then call parse call

// parse_location
// expr = parse variable
// check if . or [ next and handle accordingly

// expr combine expr lhs and rhs

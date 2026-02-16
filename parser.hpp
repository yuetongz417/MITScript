#pragma once

#include "lexer.hpp"
#include "ast.hpp"
#include <string>
#include <map>
#include <memory>
#include <vector>
#include <optional>

class Parser
{
public:
    explicit Parser(const std::vector<Token> &tokens);

    std::optional<std::unique_ptr<ast::ASTNode>> parse();

private:
    const std::vector<Token> &tokens_;
    size_t current_;

    // Helper methods
    bool isAtEnd() const;
    const Token &peek() const;
    const Token &previous() const;
    bool check(TokenType type) const;
    bool match(std::vector<TokenType> types);
    Token advance();
    void synchronize();
    bool consume(TokenType type, const std::string &message);

    // Grammar rules
    std::unique_ptr<ast::ASTNode> program();
    std::unique_ptr<ast::ASTNode> statement();
    std::unique_ptr<ast::ASTNode> block();
    std::unique_ptr<ast::ASTNode> expression();
    std::unique_ptr<ast::ASTNode> location();

    // Statement types
    std::unique_ptr<ast::ASTNode> ifStatement();
    std::unique_ptr<ast::ASTNode> whileStatement();
    std::unique_ptr<ast::ASTNode> returnStatement();
    std::unique_ptr<ast::ASTNode> functionDeclaration();
    std::unique_ptr<ast::ASTNode> globalDeclaration();
    std::unique_ptr<ast::ASTNode> record();

    std::unique_ptr<ast::ASTNode> idTerm();
    std::unique_ptr<ast::ASTNode> assignment(std::unique_ptr<ast::ASTNode> loc);
    std::unique_ptr<ast::ASTNode> parse_assignment_or_call();
    std::unique_ptr<ast::ASTNode> function_call(std::unique_ptr<ast::ASTNode> expr);
    std::unique_ptr<ast::ASTNode> parse_location_or_call();

    std::unique_ptr<ast::ASTNode> logical_or();     // Lowest precedence: |
    std::unique_ptr<ast::ASTNode> logical_and();    // &
    std::unique_ptr<ast::ASTNode> logical_not();    // !
    std::unique_ptr<ast::ASTNode> equality();       // ==
    std::unique_ptr<ast::ASTNode> relational();     // <, >, <=, >=
    std::unique_ptr<ast::ASTNode> additive();       // +, -
    std::unique_ptr<ast::ASTNode> multiplicative(); // *, /
    std::unique_ptr<ast::ASTNode> unary();          // unary - (highest precedence)
    std::unique_ptr<ast::ASTNode> primary();        // literals, identifiers, ()
};

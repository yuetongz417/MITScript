#include "parser.hpp"

#include <iostream>
#include <algorithm>
#include <optional>

Parser::Parser(const std::vector<Token> &tokens) : tokens_(tokens), current_(0) {}

std::optional<std::unique_ptr<ast::ASTNode>> Parser::parse()
{
    try
    {
        return program();
    }
    catch (const std::exception &e)
    {
        // Print the error message using std::cerr
        std::cerr << "Caught exception: " << e.what() << std::endl;
        return std::nullopt;
    }
}

// Grammar rules
std::unique_ptr<ast::ASTNode> Parser::program()
{
    std::vector<std::unique_ptr<ast::ASTNode>> statements;

    while (!isAtEnd())
    {
        auto stmt = statement();
        if (stmt)
        {
            statements.push_back(std::move(stmt));
        }
    }

    return std::make_unique<ast::Block>(std::move(statements));
}

std::unique_ptr<ast::ASTNode> Parser::statement()
{
    // Check if the current token is a keyword
    if (check(TokenType::Keyword))
    {
        std::string keyword = peek().str;
        if (keyword == "if")
        {
            return ifStatement();
        }
        else if (keyword == "while")
        {
            return whileStatement();
        }
        else if (keyword == "return")
        {
            return returnStatement();
        }
        else if (keyword == "global")
        {
            return globalDeclaration();
        }
    }

    // Parse assignment or call statement
    return parse_assignment_or_call();
}

std::unique_ptr<ast::ASTNode> Parser::parse_assignment_or_call()
{
    // First try to parse a location (could be complex with dots, brackets, etc.)
    auto expr = location();

    if (!expr)
    {
        throw std::runtime_error("Invalid expression");
    }

    if (check(TokenType::Assign))
    {
        return assignment(std::move(expr));
    }

    if (check(TokenType::LParen))
    {
        expr = function_call(std::move(expr));
        if (!consume(TokenType::Semicolon, "Expect ';' after arguments"))
        {
            throw std::runtime_error("Invalid call expression");
        }
        return expr;
    }
    throw std::runtime_error("Invalid expression");
}

std::unique_ptr<ast::ASTNode> Parser::function_call(std::unique_ptr<ast::ASTNode> expr)
{
    consume(TokenType::LParen, "Expect '(' after function name");
    // Function call
    std::vector<std::unique_ptr<ast::ASTNode>> args;
    if (!check(TokenType::RParen))
    {
        do
        {
            auto arg = expression();
            if (!arg)
                throw std::runtime_error("Invalid argument expression");
            args.push_back(std::move(arg));
        } while (match({TokenType::Comma}));
    }
    // Functions can have zero arguments according to the spec

    if (!consume(TokenType::RParen, "Expect ')' after arguments"))
    {
        throw std::runtime_error("Invalid call expression");
    }
    return std::make_unique<ast::Call>(std::move(expr), std::move(args));
}

std::unique_ptr<ast::ASTNode> Parser::location()
{
    auto expr = idTerm();
    if (!expr)
    {
        throw std::runtime_error("Invalid expression");
    }

    while (true)
    {
        if (match({TokenType::Dot}))
        {
            // Field access
            if (!check(TokenType::Identifier))
            {
                throw std::runtime_error("Expect identifier after '.'");
            }
            std::string field = advance().str;
            expr = std::make_unique<ast::FieldDereference>(std::move(expr), std::move(field));
        }
        else if (match({TokenType::LSquareBrace}))
        {
            // Index access
            auto index = expression();
            if (!index || !consume(TokenType::RSquareBrace, "Expect ']' after index"))
            {
                throw std::runtime_error("Invalid index expression");
            }
            expr = std::make_unique<ast::IndexExpression>(std::move(expr), std::move(index));
        }
        else
        {
            break;
        }
    }
    return expr;
}

std::unique_ptr<ast::ASTNode> Parser::idTerm()
{
    if (check(TokenType::Identifier))
    {
        return std::make_unique<ast::Identifier>(advance().str);
    }
    throw std::runtime_error("Expected identifier");
}

std::unique_ptr<ast::ASTNode> Parser::assignment(std::unique_ptr<ast::ASTNode> location)
{
    if (!consume(TokenType::Assign, "Expected '='"))
    {
        throw std::runtime_error("Invalid assignment");
    }

    auto expr = expression();
    if (!expr)
    {
        throw std::runtime_error("Invalid assignment expression");
    }

    if (!consume(TokenType::Semicolon, "Expect ';' after assignment"))
    {
        throw std::runtime_error("Invalid assignment");
    }

    return std::make_unique<ast::Assignment>(std::move(location), std::move(expr));
}

std::unique_ptr<ast::ASTNode> Parser::block()
{
    if (!consume(TokenType::LBrace, "Expect '{'"))
    {
        throw std::runtime_error("Invalid block");
    }

    std::vector<std::unique_ptr<ast::ASTNode>> statements;

    while (!check(TokenType::RBrace) && !isAtEnd())
    {
        auto stmt = statement();
        if (stmt)
        {
            statements.push_back(std::move(stmt));
        }
    }

    if (!consume(TokenType::RBrace, "Expect '}'"))
    {
        throw std::runtime_error("Invalid block");
    }
    return std::make_unique<ast::Block>(std::move(statements));
}

std::unique_ptr<ast::ASTNode> Parser::ifStatement()
{
    if (!consume(TokenType::Keyword, "Expect 'if'"))
    {
        throw std::runtime_error("Invalid if statement");
    }

    if (!consume(TokenType::LParen, "Expect '(' after 'if'"))
    {
        throw std::runtime_error("Invalid if statement");
    }
    auto condition = expression();
    if (!condition || !consume(TokenType::RParen, "Expect ')' after if condition"))
    {
        throw std::runtime_error("Invalid if statement");
    }

    auto thenBranch = block();
    if (!thenBranch)
    {
        throw std::runtime_error("Invalid if statement");
    }

    std::unique_ptr<ast::ASTNode> elseBranch = nullptr;
    if (check(TokenType::Keyword) && peek().str == "else")
    {
        if (!consume(TokenType::Keyword, "Expect 'else'"))
        {
            throw std::runtime_error("Invalid else statement");
        }
        elseBranch = block();
        if (!elseBranch)
        {
            throw std::runtime_error("Invalid else block");
        }
    }

    return std::make_unique<ast::IfStatement>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
}

std::unique_ptr<ast::ASTNode> Parser::whileStatement()
{
    if (!consume(TokenType::Keyword, "Expect 'while'"))
    {
        throw std::runtime_error("Invalid while statement");
    }

    if (!consume(TokenType::LParen, "Expect '(' after 'while'"))
    {
        throw std::runtime_error("Invalid while statement");
    }
    auto condition = expression();
    if (!condition || !consume(TokenType::RParen, "Expect ')' after while condition"))
    {
        throw std::runtime_error("Invalid while statement");
    }

    auto body = block();
    if (!body)
    {
        throw std::runtime_error("Invalid while statement");
    }

    return std::make_unique<ast::WhileLoop>(std::move(condition), std::move(body));
}

std::unique_ptr<ast::ASTNode> Parser::returnStatement()
{
    if (!consume(TokenType::Keyword, "Expect 'return'"))
    {
        throw std::runtime_error("Invalid return statement");
    }

    auto expr = expression();
    if (!expr)
    {
        throw std::runtime_error("Invalid return statement");
    }

    if (!consume(TokenType::Semicolon, "Expect ';' after return value"))
    {
        throw std::runtime_error("Invalid return statement");
    }
    return std::make_unique<ast::Return>(std::move(expr));
}

std::unique_ptr<ast::ASTNode> Parser::globalDeclaration()
{
    if (!consume(TokenType::Keyword, "Expect 'global'"))
    {
        throw std::runtime_error("Invalid global statement");
    }

    if (!check(TokenType::Identifier))
    {
        throw std::runtime_error("Expect identifier after 'global'");
    }

    std::string id = advance().str;
    if (!consume(TokenType::Semicolon, "Expect ';' after global declaration"))
    {
        throw std::runtime_error("Invalid global declaration");
    }

    return std::make_unique<ast::Global>(std::move(id));
}

std::unique_ptr<ast::ASTNode> Parser::expression()
{
    // Function declaration: fun ( [id+,] ) block
    if (check(TokenType::Keyword) && peek().str == "fun")
    {
        return functionDeclaration();
    }

    // Record: { [id : expr ;]* }
    if (check(TokenType::LBrace))
    {
        return record();
    }

    // Otherwise parse as logical_or (lowest precedence)
    return logical_or();
}

// Operator precedence from spec (highest to lowest):
// - unary minus (highest)
// *, / multiplication, division
// +, - addition, subtraction
// <, <=, >=, >, == relational
// ! conditional not
// & conditional and
// | conditional or (lowest)

std::unique_ptr<ast::ASTNode> Parser::logical_or()
{
    auto expr = logical_and();

    while (match({TokenType::Or}))
    {
        auto right = logical_and();
        if (!right)
            throw std::runtime_error("Invalid logical OR expression");
        expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::Or, std::move(right));
    }

    return expr;
}

std::unique_ptr<ast::ASTNode> Parser::logical_and()
{
    auto expr = logical_not();

    while (match({TokenType::And}))
    {
        auto right = logical_not();
        if (!right)
            throw std::runtime_error("Invalid logical AND expression");
        expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::And, std::move(right));
    }

    return expr;
}

std::unique_ptr<ast::ASTNode> Parser::logical_not()
{
    if (match({TokenType::Not}))
    {
        auto expr = logical_not();
        if (!expr)
            throw std::runtime_error("Invalid logical NOT expression");
        return std::make_unique<ast::UnaryExpression>(ast::UnaryExpression::UnaryOp::Not, std::move(expr));
    }

    return equality();
}

std::unique_ptr<ast::ASTNode> Parser::equality()
{
    auto expr = relational();

    while (match({TokenType::Eq}))
    {
        auto right = relational();
        if (!right)
            throw std::runtime_error("Invalid equality expression");
        expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::Eq, std::move(right));
    }

    return expr;
}

std::unique_ptr<ast::ASTNode> Parser::relational()
{
    auto expr = additive();

    while (true)
    {
        if (match({TokenType::Lt}))
        {
            auto right = additive();
            if (!right)
                throw std::runtime_error("Invalid relational expression");
            expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::Lt, std::move(right));
        }
        else if (match({TokenType::Gt}))
        {
            auto right = additive();
            if (!right)
                throw std::runtime_error("Invalid relational expression");
            expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::Gt, std::move(right));
        }
        else if (match({TokenType::Leq}))
        {
            auto right = additive();
            if (!right)
                throw std::runtime_error("Invalid relational expression");
            expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::Leq, std::move(right));
        }
        else if (match({TokenType::Geq}))
        {
            auto right = additive();
            if (!right)
                throw std::runtime_error("Invalid relational expression");
            expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::Geq, std::move(right));
        }
        else
        {
            break;
        }
    }

    return expr;
}

std::unique_ptr<ast::ASTNode> Parser::additive()
{
    auto expr = multiplicative();

    while (true)
    {
        if (match({TokenType::Add}))
        {
            auto right = multiplicative();
            if (!right)
                throw std::runtime_error("Invalid additive expression");
            expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::Add, std::move(right));
        }
        else if (match({TokenType::Sub}))
        {
            auto right = multiplicative();
            if (!right)
                throw std::runtime_error("Invalid additive expression");
            expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::Sub, std::move(right));
        }
        else
        {
            break;
        }
    }

    return expr;
}

std::unique_ptr<ast::ASTNode> Parser::multiplicative()
{
    auto expr = unary();

    while (true)
    {
        if (match({TokenType::Mul}))
        {
            auto right = unary();
            if (!right)
                throw std::runtime_error("Invalid multiplicative expression");
            expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::Mul, std::move(right));
        }
        else if (match({TokenType::Div}))
        {
            auto right = unary();
            if (!right)
                throw std::runtime_error("Invalid multiplicative expression");
            expr = std::make_unique<ast::BinaryExpression>(std::move(expr), ast::BinaryExpression::Div, std::move(right));
        }
        else
        {
            break;
        }
    }

    return expr;
}

std::unique_ptr<ast::ASTNode> Parser::unary()
{
    // Handle unary minus (highest precedence according to spec)
    if (match({TokenType::Sub}))
    {
        auto expr = unary();
        if (!expr)
            throw std::runtime_error("Invalid unary expression");
        return std::make_unique<ast::UnaryExpression>(ast::UnaryExpression::UnaryOp::Neg, std::move(expr));
    }

    // No unary operator, parse as primary
    return primary();
}

std::unique_ptr<ast::ASTNode> Parser::primary()
{
    // Handle parenthesized expressions
    if (match({TokenType::LParen}))
    {
        auto expr = logical_or(); // Parse full expression inside parens
        if (!consume(TokenType::RParen, "Expect ')' after expression"))
        {
            throw std::runtime_error("Invalid parenthesized expression");
        }
        return expr;
    }

    // Handle literals
    if (check(TokenType::IntLiteral))
    {
        int value = std::stoi(advance().str);
        return std::make_unique<ast::IntegerConstant>(value);
    }

    if (check(TokenType::StringLiteral))
    {
        std::string value = advance().str;
        value = value.substr(1, value.length() - 2); // Remove quotes
        return std::make_unique<ast::StringConstant>(std::move(value));
    }

    if (check(TokenType::BooleanLiteral))
    {
        bool value = advance().str == "true";
        return std::make_unique<ast::BooleanConstant>(value);
    }

    if (check(TokenType::Keyword))
    {
        std::string keyword = peek().str;
        if (keyword == "None")
        {
            advance();
            return std::make_unique<ast::NoneConstant>();
        }
    }

    // Handle location (identifier, field access, index, call)
    return parse_location_or_call();
}

std::unique_ptr<ast::ASTNode> Parser::parse_location_or_call()
{
    // First try to parse a location
    auto expr = location();

    if (!expr)
    {
        throw std::runtime_error("Invalid expression");
    }
    if (check(TokenType::LParen))
    {
        return function_call(std::move(expr));
    }
    return expr;
}

std::unique_ptr<ast::ASTNode> Parser::functionDeclaration()
{
    if (!(consume(TokenType::Keyword, "Expect 'fun'")))
    {
        throw std::runtime_error("Invalid function declaration");
    }

    if (!consume(TokenType::LParen, "Expect '(' after 'fun'"))
    {
        throw std::runtime_error("Invalid function declaration");
    }

    std::vector<std::string> parameters;
    if (!check(TokenType::RParen))
    {
        do
        {
            if (!check(TokenType::Identifier))
            {
                throw std::runtime_error("Expect parameter name");
            }
            parameters.push_back(advance().str);
        } while (match({TokenType::Comma}));
    }
    // Functions can have zero parameters according to the spec

    if (!consume(TokenType::RParen, "Expect ')' after parameters"))
    {
        throw std::runtime_error("Invalid function declaration");
    }

    auto body = block();
    if (!body)
    {
        throw std::runtime_error("Invalid function body");
    }

    return std::make_unique<ast::FunctionDeclaration>(std::move(parameters), std::move(body));
}

std::unique_ptr<ast::ASTNode> Parser::record()
{
    if (!consume(TokenType::LBrace, "Expect '{'"))
    {
        throw std::runtime_error("Invalid record");
    }

    std::vector<std::pair<std::string, std::unique_ptr<ast::ASTNode>>> fields;

    while (!check(TokenType::RBrace) && !isAtEnd())
    {
        if (!check(TokenType::Identifier))
        {
            throw std::runtime_error("Expect field name");
        }

        std::string key = advance().str;

        if (!consume(TokenType::Colon, "Expect ':' after record key"))
        {
            throw std::runtime_error("Invalid record field");
        }
        auto value = expression();
        if (!value || !consume(TokenType::Semicolon, "Expect ';' after record value"))
        {
            throw std::runtime_error("Invalid record field");
        }

        fields.push_back({key, std::move(value)});
    }

    if (!consume(TokenType::RBrace, "Expect '}' after record"))
    {
        throw std::runtime_error("Invalid record");
    }
    return std::make_unique<ast::Record>(std::move(fields));
}

// Helper methods
bool Parser::isAtEnd() const
{
    return current_ >= tokens_.size() || (current_ < tokens_.size() && tokens_[current_].type == TokenType::EoF);
}

const Token &Parser::peek() const
{
    if (isAtEnd())
    {
        static Token eofToken{TokenType::None, "", -1};
        return eofToken;
    }
    return tokens_[current_];
}

const Token &Parser::previous() const
{
    if (current_ > 0)
        return tokens_[current_ - 1];

    static Token dummyToken{TokenType::Error, "", 0};
    return dummyToken;
}

bool Parser::check(TokenType type) const
{
    if (isAtEnd())
        return false;
    return peek().type == type;
}

bool Parser::match(std::vector<TokenType> types)
{
    for (TokenType type : types)
    {
        if (check(type))
        {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::advance()
{
    if (!isAtEnd())
        current_++;
    return previous();
}

bool Parser::consume(TokenType type, const std::string &message)
{
    if (check(type))
    {
        advance();
        return true;
    }
    return false;
}

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace ast
{
    class ASTNode;
    class Block;
    class Assignment;
    class Global;
    class IfStatement;
    class WhileLoop;
    class Return;
    class FunctionDeclaration;
    class BinaryExpression;
    class UnaryExpression;
    class FieldDereference;
    class IndexExpression;
    class Call;
    class Record;
    class IntegerConstant;
    class StringConstant;
    class BooleanConstant;
    class NoneConstant;
    class Identifier;

    // Visitor pattern
    class Visitor
    {
    public:
        virtual ~Visitor() = default;

        virtual void visit(BinaryExpression &expr) = 0;
        virtual void visit(UnaryExpression &expr) = 0;
        virtual void visit(FieldDereference &expr) = 0;
        virtual void visit(IndexExpression &expr) = 0;
        virtual void visit(Call &expr) = 0;
        virtual void visit(Record &expr) = 0;
        virtual void visit(IntegerConstant &expr) = 0;
        virtual void visit(StringConstant &expr) = 0;
        virtual void visit(BooleanConstant &expr) = 0;
        virtual void visit(NoneConstant &expr) = 0;
        virtual void visit(Identifier &expr) = 0;
        virtual void visit(Block &stmt) = 0;
        virtual void visit(Assignment &stmt) = 0;
        virtual void visit(Global &stmt) = 0;
        virtual void visit(IfStatement &stmt) = 0;
        virtual void visit(WhileLoop &stmt) = 0;
        virtual void visit(Return &stmt) = 0;
        virtual void visit(FunctionDeclaration &stmt) = 0;
    };

    class ASTNode
    {
    public:
        virtual void accept(Visitor &visitor) = 0;
    };

    // Statements
    class Block : public ASTNode
    {
    public:
        std::vector<std::unique_ptr<ASTNode>> statements;
        Block(std::vector<std::unique_ptr<ASTNode>> stmts) : statements(std::move(stmts)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class Assignment : public ASTNode
    {
    public:
        std::unique_ptr<ASTNode> lhs;
        std::unique_ptr<ASTNode> expr;
        Assignment(std::unique_ptr<ASTNode> l, std::unique_ptr<ASTNode> e)
            : lhs(std::move(l)), expr(std::move(e)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class Global : public ASTNode
    {
    public:
        std::string name;
        Global(std::string n) : name(std::move(n)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class IfStatement : public ASTNode
    {
    public:
        std::unique_ptr<ASTNode> condition;
        std::unique_ptr<ASTNode> thenPart;
        std::unique_ptr<ASTNode> elsePart; // Can be null
        IfStatement(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> then, std::unique_ptr<ASTNode> els = nullptr)
            : condition(std::move(cond)), thenPart(std::move(then)), elsePart(std::move(els)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class WhileLoop : public ASTNode
    {
    public:
        std::unique_ptr<ASTNode> condition;
        std::unique_ptr<ASTNode> body;
        WhileLoop(std::unique_ptr<ASTNode> cond, std::unique_ptr<ASTNode> b)
            : condition(std::move(cond)), body(std::move(b)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class Return : public ASTNode
    {
    public:
        std::unique_ptr<ASTNode> expression;
        Return(std::unique_ptr<ASTNode> expr) : expression(std::move(expr)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class FunctionDeclaration : public ASTNode
    {
    public:
        std::vector<std::string> arguments;
        std::unique_ptr<ASTNode> body;
        FunctionDeclaration(std::vector<std::string> args, std::unique_ptr<ASTNode> b)
            : arguments(std::move(args)), body(std::move(b)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    // Expressions
    class BinaryExpression : public ASTNode
    {
    public:
        enum BinaryOp
        {
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
            Or
        };

        std::unique_ptr<ASTNode> leftOperand;
        BinaryOp op;
        std::unique_ptr<ASTNode> rightOperand;

        BinaryExpression(std::unique_ptr<ASTNode> left, BinaryOp operator_, std::unique_ptr<ASTNode> right)
            : leftOperand(std::move(left)), op(operator_), rightOperand(std::move(right)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class UnaryExpression : public ASTNode
    {
    public:
        enum UnaryOp
        {
            Neg, // unary minus
            Not  // logical not
        };

        std::unique_ptr<ASTNode> operand;
        UnaryOp op;

        UnaryExpression(UnaryOp operator_, std::unique_ptr<ASTNode> operand_)
            : operand(std::move(operand_)), op(operator_) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class FieldDereference : public ASTNode
    {
    public:
        std::unique_ptr<ASTNode> baseExpression;
        std::string field;
        FieldDereference(std::unique_ptr<ASTNode> base, std::string f)
            : baseExpression(std::move(base)), field(std::move(f)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class IndexExpression : public ASTNode
    {
    public:
        std::unique_ptr<ASTNode> baseExpression;
        std::unique_ptr<ASTNode> index;
        IndexExpression(std::unique_ptr<ASTNode> base, std::unique_ptr<ASTNode> idx)
            : baseExpression(std::move(base)), index(std::move(idx)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class Call : public ASTNode
    {
    public:
        std::unique_ptr<ASTNode> targetExpression;
        std::vector<std::unique_ptr<ASTNode>> arguments;
        Call(std::unique_ptr<ASTNode> target, std::vector<std::unique_ptr<ASTNode>> args)
            : targetExpression(std::move(target)), arguments(std::move(args)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class Record : public ASTNode
    {
    public:
        std::vector<std::pair<std::string, std::unique_ptr<ast::ASTNode>>> fields;
        Record(std::vector<std::pair<std::string, std::unique_ptr<ast::ASTNode>>> f) : fields(std::move(f)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    // Constants and Identifiers
    class IntegerConstant : public ASTNode
    {
    public:
        int value;
        IntegerConstant(int v) : value(v) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class StringConstant : public ASTNode
    {
    public:
        std::string value;
        StringConstant(std::string s) : value(std::move(s)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class BooleanConstant : public ASTNode
    {
    public:
        bool value;
        BooleanConstant(bool v) : value(v) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class NoneConstant : public ASTNode
    {
    public:
        NoneConstant() {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };

    class Identifier : public ASTNode
    {
    public:
        std::string name;
        Identifier(std::string n) : name(std::move(n)) {}
        void accept(Visitor &visitor) override { visitor.visit(*this); }
    };
}

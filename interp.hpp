#pragma once

#include "parser.hpp"
#include "ast.hpp"
#include "gc/gc.hpp"

#include <string>
#include <vector>
#include <stack>
#include <optional>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <variant>
#include <set>

#define DEBUG_INTERP false

class Value;
class Record;
class Function;

class UninitializedVariableException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "UninitializedVariableException";
    }
};

class IllegalCastException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "IllegalCastException";
    }
};

class IllegalArithmeticException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "IllegalArithmeticException";
    }
};

class RuntimeException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "RuntimeException";
    }
};

class Frame : public Collectable
{
public:
    using VarMap = std::unordered_map<std::string, Value *>;

    struct GlobalInfo
    {
        std::unordered_set<std::string> globals;
        Frame *globalFrame;
    };

private:
    VarMap variables;
    Frame *parent;
    GlobalInfo global;

protected:
    void follow(CollectedHeap &heap) override;

public:
    Frame(Frame *parentAddr = nullptr)
        : parent(parentAddr), global(parentAddr ? parent->global : GlobalInfo{{}, this}) {}

    void setVar(const std::string &name, Value *addr)
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Setting variable '" << name << "' at frame " << this << std::endl;
        variables[name] = addr;
    }

    static Frame *lookupWrite(const std::string &name, Frame *currentFrame)
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] lookupWrite for '" << name << "'" << std::endl;
        if (currentFrame->global.globals.find(name) != currentFrame->global.globals.end())
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] '" << name << "' is global, using global frame" << std::endl;
            return currentFrame->global.globalFrame;
        }
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] '" << name << "' is local" << std::endl;
        return currentFrame;
    }

    static Value *lookupRead(const std::string &name, Frame *currentFrame)
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] lookupRead for '" << name << "' at frame " << currentFrame << std::endl;

        // Check if it's a global variable
        if (currentFrame->global.globals.find(name) != currentFrame->global.globals.end())
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] '" << name << "' is declared global" << std::endl;
            Frame *globalFrame = currentFrame->global.globalFrame;
            if (globalFrame->variables.find(name) != globalFrame->variables.end())
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Found '" << name << "' in global frame" << std::endl;
                return globalFrame->variables[name];
            }
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] '" << name << "' not initialized in global frame" << std::endl;
            throw UninitializedVariableException();
        }

        // Check if it's in the current frame
        if (currentFrame->variables.find(name) != currentFrame->variables.end())
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Found '" << name << "' in current frame" << std::endl;
            return currentFrame->variables[name];
        }

        // Check parent frame (if exists)
        if (currentFrame->parent == nullptr)
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] '" << name << "' not found, no parent frame" << std::endl;
            throw UninitializedVariableException();
        }

        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Looking up '" << name << "' in parent frame" << std::endl;
        return lookupRead(name, currentFrame->parent);
    }

    Frame *getParent() const { return parent; }
    void setParent(Frame *p) { parent = p; }

    void setGlobal(Frame *globalFrameAddr, const std::unordered_set<std::string> &globals)
    {
        global = GlobalInfo{globals, globalFrameAddr};
    }

    std::optional<GlobalInfo> getGlobal() const { return global; }
};

struct Record
{
    std::vector<std::pair<std::string, Value *>> fields;
};

struct Function
{
    Frame *context;
    std::vector<std::string> arguments;
    ast::Block *body;
};

class Value : public Collectable
{
public:
    using VariantType = std::variant<bool,
                                     int,
                                     std::string,
                                     Record *,
                                     Function *,
                                     std::monostate>;

    VariantType data;

    Value() : data(std::monostate{}) {}
    Value(bool b) : data(b) {}
    Value(int n) : data(n) {}
    Value(const std::string &s) : data(s) {}
    Value(const char *s) : data(std::string(s)) {}
    Value(Record *a) : data(a) {}
    Value(Function *f) : data(f) {}

protected:
    void follow(CollectedHeap &heap) override;
};

inline void Frame::follow(CollectedHeap &heap)
{
    // Follow all Value* pointers in the variables map
    for (auto &pair : variables)
    {
        if (pair.second != nullptr)
        {
            heap.markSuccessors(pair.second);
        }
    }

    // Follow parent frame
    if (parent != nullptr)
    {
        heap.markSuccessors(parent);
    }

    // Follow global frame if it's different from parent
    if (global.globalFrame != nullptr && global.globalFrame != parent)
    {
        heap.markSuccessors(global.globalFrame);
    }
}

inline void Value::follow(CollectedHeap &heap)
{
    // Follow pointers based on what's stored in the variant
    if (std::holds_alternative<Record *>(data))
    {
        Record *record = std::get<Record *>(data);
        if (record != nullptr)
        {
            // Follow all Value* pointers in the record's fields
            for (auto &field : record->fields)
            {
                if (field.second != nullptr)
                {
                    heap.markSuccessors(field.second);
                }
            }
        }
    }
    else if (std::holds_alternative<Function *>(data))
    {
        Function *func = std::get<Function *>(data);
        if (func != nullptr && func->context != nullptr)
        {
            // Follow the function's context (Frame*)
            heap.markSuccessors(func->context);
        }
    }
}

class Interpreter : public ast::Visitor
{
public:
    void interpret(ast::ASTNode &root)
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Starting interpretation" << std::endl;

        Frame *globalFrame = new Frame();
        globalFrame->setGlobal(globalFrame, {"print", "input", "intcast", "None"});

        // Create native functions with nullptr body to mark them as native
        printNative_ = new Function{globalFrame, {"s"}, nullptr};
        inputNative_ = new Function{globalFrame, {}, nullptr};
        intcastNative_ = new Function{globalFrame, {"s"}, nullptr};

        globalFrame->setVar("print", new Value(printNative_));
        globalFrame->setVar("input", new Value(inputNative_));
        globalFrame->setVar("intcast", new Value(intcastNative_));
        NONE = new Value();
        globalFrame->setVar("None", NONE);

        stack_.push(globalFrame);
        root.accept(*this);

        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Interpretation complete" << std::endl;
    }

private:
    // Store pointers to native functions
    Function *printNative_;
    Function *inputNative_;
    Function *intcastNative_;
    Value *NONE;

    void visit(ast::IntegerConstant &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] IntegerConstant: " << expr.value << std::endl;
        rval_ = new Value(expr.value);
    }

    void visit(ast::BooleanConstant &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] BooleanConstant: " << (expr.value ? "true" : "false") << std::endl;
        rval_ = new Value(expr.value);
    }

    void visit(ast::StringConstant &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] StringConstant: \"" << expr.value << "\"" << std::endl;
        rval_ = new Value(expr.value);
    }

    void visit(ast::NoneConstant &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] NoneConstant" << std::endl;
        rval_ = NONE;
    }

    void visit(ast::BinaryExpression &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] BinaryExpression op=" << expr.op << std::endl;

        expr.leftOperand->accept(*this);
        Value *left = rval_;
        expr.rightOperand->accept(*this);
        Value *right = rval_;

        switch (expr.op)
        {
        case ast::BinaryExpression::Add:
            if (std::holds_alternative<int>(left->data) && std::holds_alternative<int>(right->data))
            {
                int result = std::get<int>(left->data) + std::get<int>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Integer addition: " << result << std::endl;
                rval_ = new Value(result);
            }
            else if (std::holds_alternative<std::string>(left->data) && std::holds_alternative<std::string>(right->data))
            {
                std::string result = std::get<std::string>(left->data) + std::get<std::string>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] String concatenation: \"" << result << "\"" << std::endl;
                rval_ = new Value(result);
            }
            else if (std::holds_alternative<std::string>(left->data))
            {
                std::string result = std::get<std::string>(left->data) + valueToString(right);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] String + cast: \"" << result << "\"" << std::endl;
                rval_ = new Value(result);
            }
            else if (std::holds_alternative<std::string>(right->data))
            {
                std::string result = valueToString(left) + std::get<std::string>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Cast + string: \"" << result << "\"" << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Add" << std::endl;
                throw IllegalCastException();
            }
            break;

        case ast::BinaryExpression::Sub:
            if (std::holds_alternative<int>(left->data) && std::holds_alternative<int>(right->data))
            {
                int result = std::get<int>(left->data) - std::get<int>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Integer subtraction: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Sub" << std::endl;
                throw IllegalCastException();
            }
            break;

        case ast::BinaryExpression::Mul:
            if (std::holds_alternative<int>(left->data) && std::holds_alternative<int>(right->data))
            {
                int result = std::get<int>(left->data) * std::get<int>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Integer multiplication: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Mul" << std::endl;
                throw IllegalCastException();
            }
            break;

        case ast::BinaryExpression::Div:
            if (std::holds_alternative<int>(left->data) && std::holds_alternative<int>(right->data))
            {
                int divisor = std::get<int>(right->data);
                if (divisor == 0)
                {
                    if (DEBUG_INTERP)
                        std::cerr << "[DEBUG] IllegalArithmeticException: division by zero" << std::endl;
                    throw IllegalArithmeticException();
                }
                int result = std::get<int>(left->data) / divisor;
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Integer division: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Div" << std::endl;
                throw IllegalCastException();
            }
            break;

        case ast::BinaryExpression::Eq:
            if (std::holds_alternative<int>(left->data) && std::holds_alternative<int>(right->data))
            {
                bool result = std::get<int>(left->data) == std::get<int>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Integer equality: " << result << std::endl;
                rval_ = new Value(result);
            }
            else if (std::holds_alternative<std::string>(left->data) && std::holds_alternative<std::string>(right->data))
            {
                bool result = std::get<std::string>(left->data) == std::get<std::string>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] String equality: " << result << std::endl;
                rval_ = new Value(result);
            }
            else if (std::holds_alternative<Record *>(left->data) && std::holds_alternative<Record *>(right->data))
            {
                bool result = std::get<Record *>(left->data) == std::get<Record *>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Record equality: " << result << std::endl;
                rval_ = new Value(result);
            }
            else if (std::holds_alternative<Function *>(left->data) && std::holds_alternative<Function *>(right->data))
            {
                Function *f1 = std::get<Function *>(left->data);
                Function *f2 = std::get<Function *>(right->data);
                bool equal = (f1->context == f2->context) &&
                             (f1->arguments == f2->arguments) &&
                             (f1->body == f2->body);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Function equality: " << equal << std::endl;
                rval_ = new Value(equal);
            }
            else if (std::holds_alternative<bool>(left->data) && std::holds_alternative<bool>(right->data))
            {
                bool result = std::get<bool>(left->data) == std::get<bool>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Boolean equality: " << result << std::endl;
                rval_ = new Value(result);
            }
            else if (std::holds_alternative<std::monostate>(left->data) && std::holds_alternative<std::monostate>(right->data))
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] None equality: true" << std::endl;
                rval_ = new Value(true);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Type mismatch equality: false" << std::endl;
                rval_ = new Value(false);
            }
            break;

        case ast::BinaryExpression::Lt:
            if (std::holds_alternative<int>(left->data) && std::holds_alternative<int>(right->data))
            {
                bool result = std::get<int>(left->data) < std::get<int>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Less than: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Lt" << std::endl;
                throw IllegalCastException();
            }
            break;

        case ast::BinaryExpression::Gt:
            if (std::holds_alternative<int>(left->data) && std::holds_alternative<int>(right->data))
            {
                bool result = std::get<int>(left->data) > std::get<int>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Greater than: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Gt" << std::endl;
                throw IllegalCastException();
            }
            break;

        case ast::BinaryExpression::Leq:
            if (std::holds_alternative<int>(left->data) && std::holds_alternative<int>(right->data))
            {
                bool result = std::get<int>(left->data) <= std::get<int>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Less than or equal: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Leq" << std::endl;
                throw IllegalCastException();
            }
            break;

        case ast::BinaryExpression::Geq:
            if (std::holds_alternative<int>(left->data) && std::holds_alternative<int>(right->data))
            {
                bool result = std::get<int>(left->data) >= std::get<int>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Greater than or equal: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Geq" << std::endl;
                throw IllegalCastException();
            }
            break;

        case ast::BinaryExpression::And:
            if (std::holds_alternative<bool>(left->data) && std::holds_alternative<bool>(right->data))
            {
                bool result = std::get<bool>(left->data) && std::get<bool>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Logical AND: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in And" << std::endl;
                throw IllegalCastException();
            }
            break;

        case ast::BinaryExpression::Or:
            if (std::holds_alternative<bool>(left->data) && std::holds_alternative<bool>(right->data))
            {
                bool result = std::get<bool>(left->data) || std::get<bool>(right->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Logical OR: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Or" << std::endl;
                throw IllegalCastException();
            }
            break;

        default:
            break;
        }
    }

    void visit(ast::UnaryExpression &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] UnaryExpression op=" << expr.op << std::endl;

        expr.operand->accept(*this);
        Value *operand = rval_;

        switch (expr.op)
        {
        case ast::UnaryExpression::Neg:
            if (std::holds_alternative<int>(operand->data))
            {
                int result = -std::get<int>(operand->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Unary negation: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Neg" << std::endl;
                throw IllegalCastException();
            }
            break;

        case ast::UnaryExpression::Not:
            if (std::holds_alternative<bool>(operand->data))
            {
                bool result = !std::get<bool>(operand->data);
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Logical NOT: " << result << std::endl;
                rval_ = new Value(result);
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException in Not" << std::endl;
                throw IllegalCastException();
            }
            break;

        default:
            break;
        }
    }

    void visit(ast::Assignment &assn) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Assignment" << std::endl;

        if (auto *ident = dynamic_cast<ast::Identifier *>(assn.lhs.get()))
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Variable assignment to '" << ident->name << "'" << std::endl;
            assn.expr->accept(*this);
            Value *v = rval_;
            Frame *targetFrame = Frame::lookupWrite(ident->name, stack_.top());
            targetFrame->setVar(ident->name, v);
        }
        else if (auto *fieldDeref = dynamic_cast<ast::FieldDereference *>(assn.lhs.get()))
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Field assignment to field '" << fieldDeref->field << "'" << std::endl;
            fieldDeref->baseExpression->accept(*this);
            Value *a1 = rval_;

            assn.expr->accept(*this);
            Value *v = rval_;

            if (!std::holds_alternative<Record *>(a1->data))
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException: field assignment on non-record" << std::endl;
                throw IllegalCastException();
            }
            Record *record = std::get<Record *>(a1->data);
            // Update existing field or append new one
            bool found = false;
            for (auto &field : record->fields)
            {
                if (field.first == fieldDeref->field)
                {
                    field.second = v;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                record->fields.push_back({fieldDeref->field, v});
            }
        }
        else if (auto *indexExpr = dynamic_cast<ast::IndexExpression *>(assn.lhs.get()))
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Index assignment" << std::endl;
            indexExpr->baseExpression->accept(*this);
            Value *a1 = rval_;

            indexExpr->index->accept(*this);
            Value *indexValue = rval_;
            std::string fieldName = valueToString(indexValue);

            assn.expr->accept(*this);
            Value *v2 = rval_;

            if (!std::holds_alternative<Record *>(a1->data))
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException: index assignment on non-record" << std::endl;
                throw IllegalCastException();
            }
            Record *record = std::get<Record *>(a1->data);
            // Update existing field or append new one
            bool found = false;
            for (auto &field : record->fields)
            {
                if (field.first == fieldName)
                {
                    field.second = v2;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                record->fields.push_back({fieldName, v2});
            }
        }
    }

    void visit(ast::IfStatement &stmt) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] IfStatement" << std::endl;

        stmt.condition->accept(*this);
        Value *v = rval_;

        if (std::holds_alternative<bool>(v->data))
        {
            bool b = std::get<bool>(v->data);
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Condition is " << (b ? "true" : "false") << std::endl;
            if (b)
            {
                stmt.thenPart->accept(*this);
            }
            else if (stmt.elsePart)
            {
                stmt.elsePart->accept(*this);
            }
        }
        else
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] IllegalCastException: if condition is not bool" << std::endl;
            throw IllegalCastException();
        }
    }

    void visit(ast::WhileLoop &stmt) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] WhileLoop" << std::endl;

        stmt.condition->accept(*this);
        Value *v = rval_;

        if (std::holds_alternative<bool>(v->data))
        {
            bool b = std::get<bool>(v->data);
            if (b)
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Executing while body" << std::endl;
                stmt.body->accept(*this);
                if (!hasReturned_)
                {
                    this->visit(stmt);
                }
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] While condition false, skipping" << std::endl;
            }
        }
        else
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] IllegalCastException: while condition is not bool" << std::endl;
            throw IllegalCastException();
        }
    }

    void visit(ast::Block &stmt) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Block with " << stmt.statements.size() << " statements" << std::endl;

        for (auto &statement : stmt.statements)
        {
            statement->accept(*this);
            if (hasReturned_)
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Return encountered, skipping rest of block" << std::endl;
                break;
            }
        }
    }

    void visit(ast::Return &stmt) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Return statement" << std::endl;
        stmt.expression->accept(*this);
        hasReturned_ = true;
    }

    void visit(ast::Global &stmt) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Global declaration: " << stmt.name << std::endl;
        // No-op in terms of execution
    }

    void visit(ast::Identifier &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Identifier: " << expr.name << std::endl;
        rval_ = Frame::lookupRead(expr.name, stack_.top());
    }

    void visit(ast::Record &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Record with " << expr.fields.size() << " fields" << std::endl;

        Record *r = new Record();
        for (auto &field : expr.fields)
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Evaluating field '" << field.first << "'" << std::endl;
            field.second->accept(*this);
            r->fields.push_back({field.first, rval_});
        }
        rval_ = new Value(r);
    }

    void visit(ast::FieldDereference &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] FieldDereference: field '" << expr.field << "'" << std::endl;

        expr.baseExpression->accept(*this);
        Value *a1 = rval_;

        if (std::holds_alternative<Record *>(a1->data))
        {
            Record *record = std::get<Record *>(a1->data);
            // Search for field in vector
            for (auto &field : record->fields)
            {
                if (field.first == expr.field)
                {
                    if (DEBUG_INTERP)
                        std::cerr << "[DEBUG] Field '" << expr.field << "' found" << std::endl;
                    rval_ = field.second;
                    return;
                }
            }
            // Field not found
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Field '" << expr.field << "' not found, returning None" << std::endl;
            rval_ = new Value();
        }
        else
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] IllegalCastException: field dereference on non-record" << std::endl;
            throw IllegalCastException();
        }
    }

    void visit(ast::IndexExpression &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] IndexExpression" << std::endl;

        expr.baseExpression->accept(*this);
        Value *a1 = rval_;

        if (std::holds_alternative<Record *>(a1->data))
        {
            Record *record = std::get<Record *>(a1->data);
            expr.index->accept(*this);
            Value *indexValue = rval_;
            std::string fieldName = valueToString(indexValue);

            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Index field name: '" << fieldName << "'" << std::endl;

            // Search for field in vector
            for (auto &field : record->fields)
            {
                if (field.first == fieldName)
                {
                    if (DEBUG_INTERP)
                        std::cerr << "[DEBUG] Index field found" << std::endl;
                    rval_ = field.second;
                    return;
                }
            }
            // Field not found
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Index field not found, returning None" << std::endl;
            rval_ = new Value();
        }
        else
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] IllegalCastException: index on non-record" << std::endl;
            throw IllegalCastException();
        }
    }

    void visit(ast::FunctionDeclaration &stmt) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] FunctionDeclaration with " << stmt.arguments.size() << " arguments" << std::endl;

        Function *f = new Function{
            stack_.top(),
            stmt.arguments,
            dynamic_cast<ast::Block *>(stmt.body.get())};

        rval_ = new Value(f);
    }

    void visit(ast::Call &expr) override
    {
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Function call with " << expr.arguments.size() << " arguments" << std::endl;

        expr.targetExpression->accept(*this);
        Value *target = rval_;

        if (!std::holds_alternative<Function *>(target->data))
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] IllegalCastException: calling non-function" << std::endl;
            throw IllegalCastException();
        }

        Function *f = std::get<Function *>(target->data);

        if (f->arguments.size() != expr.arguments.size())
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] RuntimeException: argument count mismatch (expected "
                          << f->arguments.size() << ", got " << expr.arguments.size() << ")" << std::endl;
            throw RuntimeException();
        }

        // Check if this is a native function by comparing pointers
        if (f == printNative_)
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Executing native print()" << std::endl;
            expr.arguments[0]->accept(*this);
            std::cout << valueToString(rval_) << std::endl;
            rval_ = new Value();
            return;
        }
        else if (f == inputNative_)
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Executing native input()" << std::endl;
            std::string input;
            std::getline(std::cin, input);
            rval_ = new Value(input);
            return;
        }
        else if (f == intcastNative_)
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Executing native intcast()" << std::endl;
            expr.arguments[0]->accept(*this);
            if (std::holds_alternative<int>(rval_->data))
            {
                rval_ = new Value(std::get<int>(rval_->data));
            }
            else if (std::holds_alternative<std::string>(rval_->data))
            {
                std::string str = std::get<std::string>(rval_->data);
                if (str.empty() || (str[0] != '-' && !isdigit(str[0])))
                {
                    if (DEBUG_INTERP)
                        std::cerr << "[DEBUG] IllegalCastException: intcast invalid string" << std::endl;
                    throw IllegalCastException();
                }
                for (size_t i = 1; i < str.length(); i++)
                {
                    if (!isdigit(str[i]))
                    {
                        if (DEBUG_INTERP)
                            std::cerr << "[DEBUG] IllegalCastException: intcast invalid string" << std::endl;
                        throw IllegalCastException();
                    }
                }
                rval_ = new Value(std::atoi(str.c_str()));
            }
            else
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] IllegalCastException: intcast on invalid type" << std::endl;
                throw IllegalCastException();
            }
            return;
        }

        // Regular function call - evaluate arguments first
        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Evaluating arguments for user-defined function" << std::endl;

        std::vector<Value *> args;
        for (size_t i = 0; i < expr.arguments.size(); i++)
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Evaluating argument " << i << std::endl;
            expr.arguments[i]->accept(*this);
            args.push_back(rval_);
        }

        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Creating new frame for function call" << std::endl;

        Frame *newFrame = new Frame(f->context);

        // Extract globals from function body
        std::unordered_set<std::string> globals = extractGlobals(f->body);

        Frame *contextGlobalFrame = f->context->getGlobal()->globalFrame;

        newFrame->setGlobal(contextGlobalFrame, globals);

        // Initialize all assigned variables to None (except function parameters)
        std::unordered_set<std::string> assigns = extractAssigns(f->body);
        for (const auto &varName : assigns)
        {
            // Don't initialize parameters or global variables
            if (std::find(f->arguments.begin(), f->arguments.end(), varName) == f->arguments.end() &&
                globals.find(varName) == globals.end())
            {
                if (DEBUG_INTERP)
                    std::cerr << "[DEBUG] Initializing local variable '" << varName << "' to None" << std::endl;
                newFrame->setVar(varName, new Value());
            }
        }

        // Set function parameters
        for (size_t i = 0; i < f->arguments.size(); i++)
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Setting parameter '" << f->arguments[i] << "'" << std::endl;
            newFrame->setVar(f->arguments[i], args[i]);
        }

        stack_.push(newFrame);

        bool oldReturnState = hasReturned_;
        hasReturned_ = false;

        f->body->accept(*this);

        if (!hasReturned_)
        {
            if (DEBUG_INTERP)
                std::cerr << "[DEBUG] Function ended without return, returning None" << std::endl;
            rval_ = new Value();
        }

        hasReturned_ = oldReturnState;
        stack_.pop();

        if (DEBUG_INTERP)
            std::cerr << "[DEBUG] Function call complete" << std::endl;
    }

    std::string valueToString(Value *v)
    {
        if (std::holds_alternative<std::string>(v->data))
        {
            return std::get<std::string>(v->data);
        }
        else if (std::holds_alternative<int>(v->data))
        {
            return std::to_string(std::get<int>(v->data));
        }
        else if (std::holds_alternative<bool>(v->data))
        {
            return std::get<bool>(v->data) ? "true" : "false";
        }
        else if (std::holds_alternative<std::monostate>(v->data))
        {
            return "None";
        }
        else if (std::holds_alternative<Function *>(v->data))
        {
            return "FUNCTION";
        }
        else if (std::holds_alternative<Record *>(v->data))
        {
            Record *record = std::get<Record *>(v->data);

            // Collect field names and sort them lexicographically for output
            std::vector<std::string> fieldNames;
            for (const auto &field : record->fields)
            {
                fieldNames.push_back(field.first);
            }
            std::sort(fieldNames.begin(), fieldNames.end());

            std::string result = "{";
            for (size_t i = 0; i < fieldNames.size(); i++)
            {
                if (i > 0)
                    result += " ";

                // Find the value for this field name
                Value *fieldValue = nullptr;
                for (const auto &field : record->fields)
                {
                    if (field.first == fieldNames[i])
                    {
                        fieldValue = field.second;
                        break;
                    }
                }

                result += fieldNames[i] + ":" + valueToString(fieldValue);
            }
            result += " }";

            return result == "{ }" ? "{}" : result;
        }
        else
        {
            throw IllegalCastException();
        }
    }

    std::unordered_set<std::string> extractGlobals(ast::ASTNode *node)
    {
        std::unordered_set<std::string> result;

        if (auto *block = dynamic_cast<ast::Block *>(node))
        {
            for (auto &stmt : block->statements)
            {
                auto stmtGlobals = extractGlobals(stmt.get());
                result.insert(stmtGlobals.begin(), stmtGlobals.end());
            }
        }
        else if (auto *globalStmt = dynamic_cast<ast::Global *>(node))
        {
            result.insert(globalStmt->name);
        }
        else if (auto *ifStmt = dynamic_cast<ast::IfStatement *>(node))
        {
            auto thenGlobals = extractGlobals(ifStmt->thenPart.get());
            result.insert(thenGlobals.begin(), thenGlobals.end());

            if (ifStmt->elsePart)
            {
                auto elseGlobals = extractGlobals(ifStmt->elsePart.get());
                result.insert(elseGlobals.begin(), elseGlobals.end());
            }
        }
        else if (auto *whileLoop = dynamic_cast<ast::WhileLoop *>(node))
        {
            auto bodyGlobals = extractGlobals(whileLoop->body.get());
            result.insert(bodyGlobals.begin(), bodyGlobals.end());
        }

        return result;
    }

    std::unordered_set<std::string> extractAssigns(ast::ASTNode *node)
    {
        std::unordered_set<std::string> result;

        if (auto *block = dynamic_cast<ast::Block *>(node))
        {
            for (auto &stmt : block->statements)
            {
                auto stmtAssigns = extractAssigns(stmt.get());
                result.insert(stmtAssigns.begin(), stmtAssigns.end());
            }
        }
        else if (auto *assn = dynamic_cast<ast::Assignment *>(node))
        {
            if (auto *ident = dynamic_cast<ast::Identifier *>(assn->lhs.get()))
            {
                result.insert(ident->name);
            }
        }
        else if (auto *ifStmt = dynamic_cast<ast::IfStatement *>(node))
        {
            auto thenAssigns = extractAssigns(ifStmt->thenPart.get());
            result.insert(thenAssigns.begin(), thenAssigns.end());

            if (ifStmt->elsePart)
            {
                auto elseAssigns = extractAssigns(ifStmt->elsePart.get());
                result.insert(elseAssigns.begin(), elseAssigns.end());
            }
        }
        else if (auto *whileLoop = dynamic_cast<ast::WhileLoop *>(node))
        {
            auto bodyAssigns = extractAssigns(whileLoop->body.get());
            result.insert(bodyAssigns.begin(), bodyAssigns.end());
        }

        return result;
    }

    Value *rval_;
    std::stack<Frame *> stack_;
    bool hasReturned_ = false;
};

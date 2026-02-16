#include "cli.hpp"

#include "bytecode/parser.hpp"
#include "bytecode/prettyprinter.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include "interp.hpp"

#include <sstream>
#include <fstream>

#include <optional>
#include <iostream>
#include <stack>

static std::string
read_istream(std::istream &is)
{
  return std::string(std::istreambuf_iterator<char>(is),
                     std::istreambuf_iterator<char>());
}

int main(int argc, char **argv)
{
  Command command = cli_parse(argc, argv);

  std::string contents = read_istream(*command.input_stream);
  std::string input_filename = command.input_filename;
  std::string output_filename = command.output_filename;

  // TODO: Remove this debug print
  // std::cout << "Input filename: " << input_filename << "\n"
  //           << "Input file contents: " << contents << "\n"
  //           << "Output filename: " << output_filename << "\n";

  Lexer lexer = Lexer(contents);
  std::vector<Token> tokens;
  Interpreter interpreter;

  switch (command.kind)
  {
  case CommandKind::SCAN:
    tokens = lexer.lex();
    lexer.printTokens(*command.output_stream);
    if (std::any_of(tokens.begin(), tokens.end(), [](const Token &token)
                    { return token.type == TokenType::Error; }))
    {
      return 1;
    }
    break;
  case CommandKind::PARSE:
    tokens = lexer.lex();
    if (!std::any_of(tokens.begin(), tokens.end(), [](const Token &token)
                     { return token.type == TokenType::Error; }))
    {
      Parser parser(tokens);
      auto ast = parser.parse();
      if (!ast.has_value())
      {
        std::cout << "parse error" << std::endl;
        return 1;
      }
    }
    else
    {
      // lexer.printErrors(std::cerr);
      return 1;
    }
    break;
  case CommandKind::COMPILE:
    std::cerr << "Error: Compile command not yet implemented" << std::endl;
    break;
  case CommandKind::INTERPRET:
    tokens = lexer.lex();
    if (!std::any_of(tokens.begin(), tokens.end(), [](const Token &token)
                     { return token.type == TokenType::Error; }))
    {
      Parser parser(tokens);
      auto ast = parser.parse();
      if (!ast.has_value())
      {
        std::cout << "parse error" << std::endl;
        return 1;
      }
      else
      {
        try
        {
          interpreter.interpret(*ast.value());
        }
        catch (const std::exception &e)
        {
          std::cerr << e.what() << std::endl;
          return 1;
        }
      }
    }
    else
    {
      lexer.printErrors(std::cerr);
      return 1;
    }
    break;
  case CommandKind::VM:
  {
    auto bytecode = bytecode::parse(contents);
    bytecode::prettyprint(bytecode, *command.output_stream);
    break;
  }
  }

  return 0;
}

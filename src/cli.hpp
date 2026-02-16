#pragma once

#include <iostream>

enum class CommandKind { SCAN, PARSE, COMPILE, INTERPRET, VM };

struct Command {
  CommandKind kind;
  std::istream *input_stream;
  std::ostream *output_stream;
  std::string input_filename;
  std::string output_filename;
  size_t mem;

  Command()
      : input_stream(nullptr), output_stream(nullptr), input_filename(""), output_filename("") {}

  ~Command() {
    if (input_stream && input_stream != &std::cin) {
      delete input_stream;
    }
    if (output_stream && output_stream != &std::cout) {
      delete output_stream;
    }
  }
};

Command cli_parse(int argc, char **argv);
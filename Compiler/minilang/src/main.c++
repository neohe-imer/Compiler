#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include "compiler.h"
#include "vm.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

static std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("Failed to open file: " + path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

int main(int argc, char** argv) {
  try {
    std::string mode = "--mode=interp";
    std::string path;

    for (int i = 1; i < argc; i++) {
      std::string a = argv[i];
      if (a.rfind("--mode=", 0) == 0) mode = a;
      else if (a == "--help" || a == "-h") {
        std::cout << "Usage: minilang [--mode=interp|--mode=vm] file\n";
        return 0;
      }
      else if (a.rfind("-", 0) == 0 || !path.empty()) {
        std::cerr << "Invalid argument: " << a << "\n";
        return 2;
      }
      else path = a;
    }

    if (path.empty()) {
      std::cerr << "Usage: minilang [--mode=interp|--mode=vm] file\n";
      return 2;
    }

    if (mode != "--mode=interp" && mode != "--mode=vm") {
      std::cerr << "Unknown mode: " << mode << "\n";
      return 2;
    }

    auto source = readFile(path);

    Lexer lexer(source);
    auto tokens = lexer.scanTokens();

    Parser parser(tokens);
    auto program = parser.parse();

    if (mode == "--mode=interp") {
      Interpreter interp;
      interp.run(program);
    } else if (mode == "--mode=vm") {
      Compiler compiler;
      auto compiled = compiler.compile(program);
      VM vm;
      vm.run(compiled);
    } else {
      throw std::runtime_error("Unknown mode: " + mode);
    }

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

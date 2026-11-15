#include "compiler.h"
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
int checks = 0;
struct Capture {
  std::ostringstream output;
  std::streambuf* old = std::cout.rdbuf(output.rdbuf());
  ~Capture() { std::cout.rdbuf(old); }
};
std::vector<StmtPtr> parse(const std::string& source) {
  return Parser(Lexer(source).scanTokens()).parse();
}
void expectError(const std::function<void()>& action, const std::string& message) {
  try { action(); }
  catch (const std::exception& error) {
    if (std::string(error.what()).find(message) == std::string::npos)
      throw std::runtime_error("Unexpected error: " + std::string(error.what()));
    ++checks;
    return;
  }
  throw std::runtime_error("Expected error: " + message);
}
void check(const std::string& source, const std::string& expected) {
  auto ast = parse(source);
  for (bool compiled : {false, true}) {
    Capture capture;
    if (compiled) { VM vm; vm.run(Compiler().compile(ast)); }
    else { Interpreter interpreter; interpreter.run(ast); }
    if (capture.output.str() != expected)
      throw std::runtime_error(std::string(compiled ? "VM" : "Interpreter") +
                               " output mismatch for: " + source);
    ++checks;
  }
}
void errorBoth(const std::string& source, const std::string& message) {
  auto ast = parse(source);
  expectError([&] { Interpreter().run(ast); }, message);
  expectError([&] { VM().run(Compiler().compile(ast)); }, message);
}
}

int main() {
  try {
    check("", "");
    check("print 1 + 2 * 3; print (1 + 2) * 3; print -4 / 2;", "7.000000\n9.000000\n-2.000000\n");
    check("print 3 > 2; print 2 >= 2; print 2 <= 2; print 1 != 2; print 1 == 1; print !0;",
          "true\ntrue\ntrue\ntrue\ntrue\ntrue\n");
    check("let x; print x; let y = 1; print x = y = 7; print y; { let x = 8; } print x;",
          "0.000000\n7.000000\n7.000000\n8.000000\n");
    check("if (false) print 9; else print 1; if (0) print 9; print 2;", "1.000000\n2.000000\n");
    check("let x = 0; while (x < 3) { let y = 0; while (y < 2) { print x * 2 + y; y = y + 1; } x = x + 1; } while (false) print 9;",
          "0.000000\n1.000000\n2.000000\n3.000000\n4.000000\n5.000000\n");
    check("let x = 0; print false && (x = 1); print true || (x = 2); print x; print true && (x = 3); print false || (x = 4); print x;",
          "false\ntrue\n0.000000\ntrue\ntrue\n4.000000\n");
    check("print false && missing; print true || missing; print 0 || 2 && 3; print 1 && 0; print !(false || true && false);",
          "false\ntrue\ntrue\nfalse\ntrue\n");
    check("// comment\r\nprint 1.25; // EOF", "1.250000\n");
    errorBoth("print missing;", "Undefined variable: missing");
    errorBoth("print true + 1;", "Expected number");
    errorBoth("print true && missing;", "Undefined variable: missing");
    errorBoth("print false || missing;", "Undefined variable: missing");
    expectError([] { parse("print 1"); }, "Expected ';'");
    expectError([] { parse("{ print 1;"); }, "Expected '}'");
    expectError([] { parse("1 = 2;"); }, "Invalid assignment target");
    expectError([] { parse("print @;"); }, "line 1");

    // Code positions must not truncate at 65536, even when individual jumps fit.
    std::string large;
    for (int i = 0; i < 17000; ++i) large += "0;";
    check(large + "let x = 0; while (x < 1) { x = x + 1; } if (true) print x;", "1.000000\n");
    std::string body;
    for (int i = 0; i < 8200; ++i) body += "0;";
    expectError([&] { Compiler().compile(parse("if (true) {" + body + "}")); }, "Jump exceeds");
    expectError([&] { Compiler().compile(parse("while (false) {" + body + "}")); }, "Jump exceeds");
    std::string constants;
    for (int i = 0; i < 65536; ++i) constants += "0;";
    auto ast = parse(constants);
    Compiler compiler;
    if (compiler.compile(ast).chunk.constants.size() != 65536) throw std::runtime_error("Constant boundary");
    ++checks;
    expectError([&] { compiler.compile(parse(constants + "0;")); }, "Too many constants");
    std::string names;
    for (int i = 0; i < 65536; ++i) names += "v" + std::to_string(i) + ";";
    if (compiler.compile(parse(names)).namePool.size() != 65536)
      throw std::runtime_error("Name boundary");
    ++checks;
    expectError([&] { compiler.compile(parse(names + "overflow;")); }, "Too many variable names");
    // Reuse after failure must clear compiler state.
    VM vm;
    { Capture capture; vm.run(compiler.compile(parse("print 2;")));
      if (capture.output.str() != "2.000000\n") throw std::runtime_error("Compiler reuse"); }
    ++checks;
    vm.run(compiler.compile(parse("let x = 1;")));
    expectError([&] { vm.run(compiler.compile(parse("print x;"))); }, "Undefined variable");

    auto malformed = [&](std::vector<uint8_t> code, const std::string& error) {
      expectError([&] { vm.run(CompiledProgram{Chunk{code, {}}, {}}); }, error);
    };
    malformed({static_cast<uint8_t>(Op::CONST)}, "Truncated");
    malformed({static_cast<uint8_t>(Op::POP)}, "Stack underflow");
    malformed({255}, "Invalid bytecode opcode");
    malformed({static_cast<uint8_t>(Op::JMP), 0xff, 0x7f}, "Invalid bytecode jump target");
    malformed({static_cast<uint8_t>(Op::JMP), 0x00, 0x80}, "Invalid bytecode jump target");
    malformed({}, "missing HALT");
    expectError([&] { vm.run(CompiledProgram{Chunk{{static_cast<uint8_t>(Op::CONST), 0, 0}, {}}, {}}); }, "");
    expectError([&] { vm.run(CompiledProgram{Chunk{{static_cast<uint8_t>(Op::LOAD), 0, 0}, {}}, {}}); }, "");
    std::cout << checks << " checks passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}

#include "vm.h"
#include <iostream>
#include <stdexcept>

static uint16_t readU16(const std::vector<uint8_t>& code, size_t& ip) {
  uint16_t lo = code[ip++];
  uint16_t hi = code[ip++];
  return (uint16_t)(lo | (hi << 8));
}

void VM::run(const Chunk& chunk) {
  stack_.clear();
  size_t ip = 0;

  auto& code = chunk.code;
  while (ip < code.size()) {
    auto op = (Op)code[ip++];

    switch (op) {
      case Op::CONST: {
        uint16_t idx = readU16(code, ip);
        push(chunk.constants[idx]);
      } break;

      case Op::LOAD: {
        uint16_t nameIdx = readU16(code, ip);
        // constant at nameIdx is stored as number/bool only in this toy,
        // so we store variable names as a trick: number is invalid here.
        // We'll encode names via a separate mechanism in compiler (see compiler.cpp).
        // For simplicity, we actually store variable names as NaN-boxed? No.
        // We'll store names in a parallel table: in this demo, LOAD/STORE operands are indices into a name pool
        // kept in constants as numbers is not possible.
        // => We'll instead have compiler encode names in constants as "bool" with side channel? Not.
        // To keep this file clean, we let compiler map nameIdx -> string by placing it in globals_ key list order.
        // In practice you should store strings in Value. (See compiler.cpp comment.)
        throw std::runtime_error("LOAD not wired: use STORE/LOAD via name pool in compiler.cpp (see compiler.cpp).");
      } break;

      case Op::STORE: {
        throw std::runtime_error("STORE not wired: use name pool in compiler.cpp (see compiler.cpp).");
      } break;

      case Op::ADD: { auto b = pop(); auto a = pop(); push(Value::num(a.asNum() + b.asNum())); } break;
      case Op::SUB: { auto b = pop(); auto a = pop(); push(Value::num(a.asNum() - b.asNum())); } break;
      case Op::MUL: { auto b = pop(); auto a = pop(); push(Value::num(a.asNum() * b.asNum())); } break;
      case Op::DIV: { auto b = pop(); auto a = pop(); push(Value::num(a.asNum() / b.asNum())); } break;

      case Op::NEG: { auto a = pop(); push(Value::num(-a.asNum())); } break;

      case Op::EQ: { auto b = pop(); auto a = pop(); push(Value::boolean(a.asNum() == b.asNum())); } break;
      case Op::NE: { auto b = pop(); auto a = pop(); push(Value::boolean(a.asNum() != b.asNum())); } break;
      case Op::LT: { auto b = pop(); auto a = pop(); push(Value::boolean(a.asNum() <  b.asNum())); } break;
      case Op::LE: { auto b = pop(); auto a = pop(); push(Value::boolean(a.asNum() <= b.asNum())); } break;
      case Op::GT: { auto b = pop(); auto a = pop(); push(Value::boolean(a.asNum() >  b.asNum())); } break;
      case Op::GE: { auto b = pop(); auto a = pop(); push(Value::boolean(a.asNum() >= b.asNum())); } break;

      case Op::NOT: { auto a = pop(); push(Value::boolean(!a.asBool())); } break;

      case Op::AND: { auto b = pop(); auto a = pop(); push(Value::boolean(a.asBool() && b.asBool())); } break;
      case Op::OR:  { auto b = pop(); auto a = pop(); push(Value::boolean(a.asBool() || b.asBool())); } break;

      case Op::JMP: {
        uint16_t offset = readU16(code, ip);
        ip += offset;
      } break;

      case Op::JMP_IF_FALSE: {
        uint16_t offset = readU16(code, ip);
        auto cond = pop();
        if (!cond.asBool()) ip += offset;
      } break;

      case Op::PRINT: {
        auto v = pop();
        std::cout << v.toString() << "\n";
      } break;

      case Op::POP: { (void)pop(); } break;

      case Op::HALT:
        return;
    }
  }
}
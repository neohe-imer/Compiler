#include "vm.h"
#include <iostream>
#include <stdexcept>

void VM::run(const CompiledProgram& prog) {
  stack_.clear();
  globals_.clear();

  const auto& code = prog.chunk.code;
  const auto& consts = prog.chunk.constants;
  const auto& pool = prog.namePool;

  size_t ip = 0;
  auto readU16 = [&](size_t& ipRef) -> uint16_t {
    if (code.size() - ipRef < 2) throw std::runtime_error("Truncated bytecode operand");
    uint16_t lo = code[ipRef++];
    uint16_t hi = code[ipRef++];
    return (uint16_t)(lo | (hi << 8));
  };
  auto readI16 = [&](size_t& ipRef) -> int32_t {
    const auto raw = readU16(ipRef);
    return raw < 0x8000 ? raw : static_cast<int32_t>(raw) - 0x10000;
  };

  auto jump = [&](int32_t offset) {
    const auto target = static_cast<int64_t>(ip) + offset;
    if (target < 0 || static_cast<uint64_t>(target) >= code.size())
      throw std::runtime_error("Invalid bytecode jump target");
    ip = static_cast<size_t>(target);
  };

  while (ip < code.size()) {
    Op op = (Op)code[ip++];

    switch (op) {
      case Op::CONST: {
        uint16_t idx = readU16(ip);
        push(consts.at(idx));
      } break;

      case Op::LOAD: {
        uint16_t nameIdx = readU16(ip);
        const std::string& name = pool.at(nameIdx);
        auto it = globals_.find(name);
        if (it == globals_.end()) throw std::runtime_error("Undefined variable: " + name);
        push(it->second);
      } break;

      case Op::STORE: {
        uint16_t nameIdx = readU16(ip);
        const std::string& name = pool.at(nameIdx);
        auto v = pop();
        globals_[name] = v;
        // keep it simple: STORE does not push anything back
      } break;

      case Op::ADD: binNum([](double a,double b){return a+b;}); break;
      case Op::SUB: binNum([](double a,double b){return a-b;}); break;
      case Op::MUL: binNum([](double a,double b){return a*b;}); break;
      case Op::DIV: binNum([](double a,double b){return a/b;}); break;

      case Op::NEG: { auto a = pop(); push(Value::num(-a.asNum())); } break;

      case Op::EQ: cmpNum([](double a,double b){return a==b;}); break;
      case Op::NE: cmpNum([](double a,double b){return a!=b;}); break;
      case Op::LT: cmpNum([](double a,double b){return a<b;}); break;
      case Op::LE: cmpNum([](double a,double b){return a<=b;}); break;
      case Op::GT: cmpNum([](double a,double b){return a>b;}); break;
      case Op::GE: cmpNum([](double a,double b){return a>=b;}); break;

      case Op::NOT: { auto a = pop(); push(Value::boolean(!a.asBool())); } break;
      case Op::AND: { auto b = pop(); auto a = pop(); push(Value::boolean(a.asBool() && b.asBool())); } break;
      case Op::OR:  { auto b = pop(); auto a = pop(); push(Value::boolean(a.asBool() || b.asBool())); } break;

      case Op::JMP: {
        int32_t off = readI16(ip);
        jump(off);
      } break;

      case Op::JMP_IF_FALSE: {
        int32_t off = readI16(ip);
        auto cond = pop();
        if (!cond.asBool()) jump(off);
      } break;

      case Op::PRINT: {
        auto v = pop();
        std::cout << v.toString() << "\n";
      } break;

      case Op::POP: { (void)pop(); } break;
      case Op::HALT: return;
      default: throw std::runtime_error("Invalid bytecode opcode");
    }
  }
  throw std::runtime_error("Bytecode missing HALT");
}

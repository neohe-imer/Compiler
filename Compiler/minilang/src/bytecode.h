#pragma once
#include "value.h"
#include <cstdint>
#include <string>
#include <vector>

enum class Op : uint8_t {
  CONST,
  LOAD, STORE,

  ADD, SUB, MUL, DIV,
  NEG,

  EQ, NE, LT, LE, GT, GE,
  NOT,

  AND, OR,

  JMP,
  JMP_IF_FALSE,

  PRINT,
  POP,
  HALT
};

struct Chunk {
  std::vector<uint8_t> code;
  std::vector<Value> constants;

  int addConst(Value v) {
    constants.push_back(std::move(v));
    return (int)constants.size() - 1;
  }

  void emit(Op op) { code.push_back((uint8_t)op); }
  void emitU16(uint16_t x) { code.push_back((uint8_t)(x & 0xFF)); code.push_back((uint8_t)((x >> 8) & 0xFF)); }
  void emitU8(uint8_t x) { code.push_back(x); }
};
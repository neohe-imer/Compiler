#pragma once
#include "bytecode.h"
#include <unordered_map>
#include <string>
#include <vector>

class VM {
public:
  void run(const Chunk& chunk);

private:
  std::vector<Value> stack_;
  std::unordered_map<std::string, Value> globals_;

  void push(Value v) { stack_.push_back(std::move(v)); }
  Value pop() { auto v = stack_.back(); stack_.pop_back(); return v; }
};
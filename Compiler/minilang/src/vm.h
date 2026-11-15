#pragma once
#include "compiler.h"
#include <stdexcept>

class VM {
public:
  void run(const CompiledProgram& prog);

private:
  std::vector<Value> stack_;
  std::unordered_map<std::string, Value> globals_;

  void push(Value v) { stack_.push_back(std::move(v)); }
  Value pop() {
    if (stack_.empty()) throw std::runtime_error("Stack underflow");
    auto v = stack_.back();
    stack_.pop_back();
    return v;
  }

  template <class F>
  void binNum(F f) {
    auto b = pop(); auto a = pop();
    push(Value::num(f(a.asNum(), b.asNum())));
  }

  template <class F>
  void cmpNum(F f) {
    auto b = pop(); auto a = pop();
    push(Value::boolean(f(a.asNum(), b.asNum())));
  }
};

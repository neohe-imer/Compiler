#pragma once
#include <variant>
#include <string>
#include <stdexcept>

struct Value {
  using V = std::variant<double, bool>;
  V v;

  static Value num(double x) { return Value{ x }; }
  static Value boolean(bool b) { return Value{ b }; }

  bool isNum() const { return std::holds_alternative<double>(v); }
  bool isBool() const { return std::holds_alternative<bool>(v); }

  double asNum() const {
    if (!isNum()) throw std::runtime_error("Expected number");
    return std::get<double>(v);
  }
  bool asBool() const {
    if (isBool()) return std::get<bool>(v);
    // numbers: 0 is false, non-zero true (handy for conditions)
    return asNum() != 0.0;
  }

  std::string toString() const {
    if (isNum()) {
      auto d = std::get<double>(v);
      return std::to_string(d);
    }
    return std::get<bool>(v) ? "true" : "false";
  }
};
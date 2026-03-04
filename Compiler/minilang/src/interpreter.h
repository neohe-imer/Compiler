#pragma once
#include "ast.h"
#include <unordered_map>
#include <string>

class Interpreter {
public:
  void run(const std::vector<StmtPtr>& program);

private:
  std::unordered_map<std::string, Value> globals_;

  void exec(const Stmt* s);
  Value eval(const Expr* e);

  // statement helpers
  void execBlock(const BlockStmt* b);

  // expr helpers
  static Value applyUnary(const Token& op, const Value& right);
  static Value applyBinary(const Value& left, const Token& op, const Value& right);
};
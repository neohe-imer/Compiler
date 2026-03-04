#include "interpreter.h"
#include <iostream>
#include <stdexcept>

void Interpreter::run(const std::vector<StmtPtr>& program) {
  for (auto& s : program) exec(s.get());
}

void Interpreter::exec(const Stmt* s) {
  if (auto* e = dynamic_cast<const ExprStmt*>(s)) {
    (void)eval(e->expr.get());
    return;
  }
  if (auto* p = dynamic_cast<const PrintStmt*>(s)) {
    auto v = eval(p->expr.get());
    std::cout << v.toString() << "\n";
    return;
  }
  if (auto* v = dynamic_cast<const VarDeclStmt*>(s)) {
    Value init = Value::num(0);
    if (v->initializer) init = eval(v->initializer.get());
    globals_[v->name] = init;
    return;
  }
  if (auto* b = dynamic_cast<const BlockStmt*>(s)) {
    execBlock(b);
    return;
  }
  if (auto* i = dynamic_cast<const IfStmt*>(s)) {
    if (eval(i->condition.get()).asBool()) exec(i->thenBranch.get());
    else if (i->elseBranch) exec(i->elseBranch.get());
    return;
  }
  if (auto* w = dynamic_cast<const WhileStmt*>(s)) {
    while (eval(w->condition.get()).asBool()) exec(w->body.get());
    return;
  }

  throw std::runtime_error("Unknown statement");
}

void Interpreter::execBlock(const BlockStmt* b) {
  // Minimal scoping: this example keeps it simple (globals only).
  // (You can extend this by pushing/popping an environment stack.)
  for (auto& s : b->statements) exec(s.get());
}

Value Interpreter::eval(const Expr* e) {
  if (auto* l = dynamic_cast<const LiteralExpr*>(e)) return l->value;

  if (auto* v = dynamic_cast<const VariableExpr*>(e)) {
    auto it = globals_.find(v->name);
    if (it == globals_.end()) throw std::runtime_error("Undefined variable: " + v->name);
    return it->second;
  }

  if (auto* a = dynamic_cast<const AssignExpr*>(e)) {
    auto val = eval(a->value.get());
    globals_[a->name] = val;
    return val;
  }

  if (auto* u = dynamic_cast<const UnaryExpr*>(e)) {
    auto right = eval(u->right.get());
    return applyUnary(u->op, right);
  }

  if (auto* b = dynamic_cast<const BinaryExpr*>(e)) {
    auto left = eval(b->left.get());
    auto right = eval(b->right.get());
    return applyBinary(left, b->op, right);
  }

  if (auto* lo = dynamic_cast<const LogicalExpr*>(e)) {
    auto left = eval(lo->left.get());
    if (lo->op.type == TokenType::OrOr) {
      if (left.asBool()) return Value::boolean(true);
      return Value::boolean(eval(lo->right.get()).asBool());
    } else { // &&
      if (!left.asBool()) return Value::boolean(false);
      return Value::boolean(eval(lo->right.get()).asBool());
    }
  }

  throw std::runtime_error("Unknown expression");
}

Value Interpreter::applyUnary(const Token& op, const Value& right) {
  switch (op.type) {
    case TokenType::Minus: return Value::num(-right.asNum());
    case TokenType::Bang:  return Value::boolean(!right.asBool());
    default: break;
  }
  throw std::runtime_error("Invalid unary operator");
}

Value Interpreter::applyBinary(const Value& left, const Token& op, const Value& right) {
  switch (op.type) {
    case TokenType::Plus:  return Value::num(left.asNum() + right.asNum());
    case TokenType::Minus: return Value::num(left.asNum() - right.asNum());
    case TokenType::Star:  return Value::num(left.asNum() * right.asNum());
    case TokenType::Slash: return Value::num(left.asNum() / right.asNum());

    case TokenType::EqualEqual: return Value::boolean(left.asNum() == right.asNum());
    case TokenType::BangEqual:  return Value::boolean(left.asNum() != right.asNum());
    case TokenType::Less:       return Value::boolean(left.asNum() <  right.asNum());
    case TokenType::LessEqual:  return Value::boolean(left.asNum() <= right.asNum());
    case TokenType::Greater:    return Value::boolean(left.asNum() >  right.asNum());
    case TokenType::GreaterEqual:return Value::boolean(left.asNum() >= right.asNum());

    default: break;
  }
  throw std::runtime_error("Invalid binary operator");
}
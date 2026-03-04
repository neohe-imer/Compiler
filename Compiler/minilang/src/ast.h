#pragma once
#include "token.h"
#include "value.h"
#include <memory>
#include <string>
#include <vector>
#include <utility>

struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct Expr {
  virtual ~Expr() = default;
};

struct LiteralExpr : Expr {
  Value value;
  explicit LiteralExpr(Value v) : value(std::move(v)) {}
};

struct VariableExpr : Expr {
  std::string name;
  explicit VariableExpr(std::string n) : name(std::move(n)) {}
};

struct AssignExpr : Expr {
  std::string name;
  ExprPtr value;
  AssignExpr(std::string n, ExprPtr v) : name(std::move(n)), value(std::move(v)) {}
};

struct UnaryExpr : Expr {
  Token op;
  ExprPtr right;
  UnaryExpr(Token o, ExprPtr r) : op(std::move(o)), right(std::move(r)) {}
};

struct BinaryExpr : Expr {
  ExprPtr left;
  Token op;
  ExprPtr right;
  BinaryExpr(ExprPtr l, Token o, ExprPtr r)
    : left(std::move(l)), op(std::move(o)), right(std::move(r)) {}
};

struct LogicalExpr : Expr {
  ExprPtr left;
  Token op;
  ExprPtr right;
  LogicalExpr(ExprPtr l, Token o, ExprPtr r)
    : left(std::move(l)), op(std::move(o)), right(std::move(r)) {}
};

// ---------------- Statements ----------------

struct Stmt {
  virtual ~Stmt() = default;
};

struct ExprStmt : Stmt {
  ExprPtr expr;
  explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct PrintStmt : Stmt {
  ExprPtr expr;
  explicit PrintStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct VarDeclStmt : Stmt {
  std::string name;
  ExprPtr initializer; // may be null
  VarDeclStmt(std::string n, ExprPtr init) : name(std::move(n)), initializer(std::move(init)) {}
};

struct BlockStmt : Stmt {
  std::vector<StmtPtr> statements;
  explicit BlockStmt(std::vector<StmtPtr> s) : statements(std::move(s)) {}
};

struct IfStmt : Stmt {
  ExprPtr condition;
  StmtPtr thenBranch;
  StmtPtr elseBranch; // may be null
  IfStmt(ExprPtr c, StmtPtr t, StmtPtr e)
    : condition(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
};

struct WhileStmt : Stmt {
  ExprPtr condition;
  StmtPtr body;
  WhileStmt(ExprPtr c, StmtPtr b) : condition(std::move(c)), body(std::move(b)) {}
};
#pragma once
#include "ast.h"
#include "token.h"
#include <vector>
#include <stdexcept>

class Parser {
public:
  explicit Parser(std::vector<Token> tokens);
  std::vector<StmtPtr> parse();

private:
  std::vector<Token> tokens_;
  size_t current_ = 0;

  // statements
  StmtPtr declaration();
  StmtPtr varDeclaration();
  StmtPtr statement();
  StmtPtr printStatement();
  StmtPtr expressionStatement();
  StmtPtr blockStatement();
  StmtPtr ifStatement();
  StmtPtr whileStatement();

  // expressions (precedence climbing)
  ExprPtr expression();
  ExprPtr assignment();
  ExprPtr logic_or();
  ExprPtr logic_and();
  ExprPtr equality();
  ExprPtr comparison();
  ExprPtr term();
  ExprPtr factor();
  ExprPtr unary();
  ExprPtr primary();

  // helpers
  bool match(std::initializer_list<TokenType> types);
  bool check(TokenType type) const;
  const Token& advance();
  bool isAtEnd() const;
  const Token& peek() const;
  const Token& previous() const;
  const Token& consume(TokenType type, const char* message);

  std::runtime_error error(const Token& token, const std::string& message) const;
};
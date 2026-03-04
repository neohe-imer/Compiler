#include "parser.h"

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

std::vector<StmtPtr> Parser::parse() {
  std::vector<StmtPtr> statements;
  while (!isAtEnd()) {
    statements.push_back(declaration());
  }
  return statements;
}

StmtPtr Parser::declaration() {
  if (match({TokenType::Let})) return varDeclaration();
  return statement();
}

StmtPtr Parser::varDeclaration() {
  auto nameTok = consume(TokenType::Identifier, "Expected variable name after 'let'.");
  ExprPtr init = nullptr;
  if (match({TokenType::Equal})) init = expression();
  consume(TokenType::Semicolon, "Expected ';' after variable declaration.");
  return std::make_unique<VarDeclStmt>(nameTok.lexeme, std::move(init));
}

StmtPtr Parser::statement() {
  if (match({TokenType::Print})) return printStatement();
  if (match({TokenType::LBrace})) return blockStatement();
  if (match({TokenType::If})) return ifStatement();
  if (match({TokenType::While})) return whileStatement();
  return expressionStatement();
}

StmtPtr Parser::printStatement() {
  auto expr = expression();
  consume(TokenType::Semicolon, "Expected ';' after print.");
  return std::make_unique<PrintStmt>(std::move(expr));
}

StmtPtr Parser::expressionStatement() {
  auto expr = expression();
  consume(TokenType::Semicolon, "Expected ';' after expression.");
  return std::make_unique<ExprStmt>(std::move(expr));
}

StmtPtr Parser::blockStatement() {
  std::vector<StmtPtr> stmts;
  while (!check(TokenType::RBrace) && !isAtEnd()) {
    stmts.push_back(declaration());
  }
  consume(TokenType::RBrace, "Expected '}' after block.");
  return std::make_unique<BlockStmt>(std::move(stmts));
}

StmtPtr Parser::ifStatement() {
  consume(TokenType::LParen, "Expected '(' after 'if'.");
  auto cond = expression();
  consume(TokenType::RParen, "Expected ')' after if condition.");
  auto thenBranch = statement();
  StmtPtr elseBranch = nullptr;
  if (match({TokenType::Else})) elseBranch = statement();
  return std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch), std::move(elseBranch));
}

StmtPtr Parser::whileStatement() {
  consume(TokenType::LParen, "Expected '(' after 'while'.");
  auto cond = expression();
  consume(TokenType::RParen, "Expected ')' after while condition.");
  auto body = statement();
  return std::make_unique<WhileStmt>(std::move(cond), std::move(body));
}

// ---------------- Expressions ----------------

ExprPtr Parser::expression() { return assignment(); }

ExprPtr Parser::assignment() {
  auto expr = logic_or();
  if (match({TokenType::Equal})) {
    auto equals = previous();
    auto value = assignment();

    if (auto* var = dynamic_cast<VariableExpr*>(expr.get())) {
      std::string name = var->name;
      return std::make_unique<AssignExpr>(std::move(name), std::move(value));
    }
    throw error(equals, "Invalid assignment target.");
  }
  return expr;
}

ExprPtr Parser::logic_or() {
  auto expr = logic_and();
  while (match({TokenType::OrOr})) {
    auto op = previous();
    auto right = logic_and();
    expr = std::make_unique<LogicalExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::logic_and() {
  auto expr = equality();
  while (match({TokenType::AndAnd})) {
    auto op = previous();
    auto right = equality();
    expr = std::make_unique<LogicalExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::equality() {
  auto expr = comparison();
  while (match({TokenType::EqualEqual, TokenType::BangEqual})) {
    auto op = previous();
    auto right = comparison();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::comparison() {
  auto expr = term();
  while (match({TokenType::Less, TokenType::LessEqual, TokenType::Greater, TokenType::GreaterEqual})) {
    auto op = previous();
    auto right = term();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::term() {
  auto expr = factor();
  while (match({TokenType::Plus, TokenType::Minus})) {
    auto op = previous();
    auto right = factor();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::factor() {
  auto expr = unary();
  while (match({TokenType::Star, TokenType::Slash})) {
    auto op = previous();
    auto right = unary();
    expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
  }
  return expr;
}

ExprPtr Parser::unary() {
  if (match({TokenType::Bang, TokenType::Minus})) {
    auto op = previous();
    auto right = unary();
    return std::make_unique<UnaryExpr>(op, std::move(right));
  }
  return primary();
}

ExprPtr Parser::primary() {
  if (match({TokenType::True}))  return std::make_unique<LiteralExpr>(Value::boolean(true));
  if (match({TokenType::False})) return std::make_unique<LiteralExpr>(Value::boolean(false));

  if (match({TokenType::Number})) {
    double n = std::get<double>(previous().literal);
    return std::make_unique<LiteralExpr>(Value::num(n));
  }

  if (match({TokenType::Identifier})) {
    return std::make_unique<VariableExpr>(previous().lexeme);
  }

  if (match({TokenType::LParen})) {
    auto expr = expression();
    consume(TokenType::RParen, "Expected ')' after expression.");
    return expr;
  }

  throw error(peek(), "Expected expression.");
}

// ---------------- Helpers ----------------

bool Parser::match(std::initializer_list<TokenType> types) {
  for (auto t : types) {
    if (check(t)) { advance(); return true; }
  }
  return false;
}

bool Parser::check(TokenType type) const {
  if (isAtEnd()) return false;
  return peek().type == type;
}

const Token& Parser::advance() {
  if (!isAtEnd()) current_++;
  return previous();
}

bool Parser::isAtEnd() const { return peek().type == TokenType::EndOfFile; }

const Token& Parser::peek() const { return tokens_[current_]; }
const Token& Parser::previous() const { return tokens_[current_ - 1]; }

const Token& Parser::consume(TokenType type, const char* message) {
  if (check(type)) return advance();
  throw error(peek(), message);
}

std::runtime_error Parser::error(const Token& token, const std::string& message) const {
  return std::runtime_error("Parse error at line " + std::to_string(token.line) +
                            " near '" + token.lexeme + "': " + message);
}
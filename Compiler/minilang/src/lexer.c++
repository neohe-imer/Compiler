#include "lexer.h"
#include <cctype>
#include <unordered_map>
#include <stdexcept>

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

bool Lexer::isAtEnd() const { return current_ >= source_.size(); }

char Lexer::advance() { return source_[current_++]; }

char Lexer::peek() const { return isAtEnd() ? '\0' : source_[current_]; }

char Lexer::peekNext() const {
  if (current_ + 1 >= source_.size()) return '\0';
  return source_[current_ + 1];
}

bool Lexer::match(char expected) {
  if (isAtEnd()) return false;
  if (source_[current_] != expected) return false;
  current_++;
  return true;
}

void Lexer::addToken(TokenType type) {
  Token t;
  t.type = type;
  t.lexeme = source_.substr(start_, current_ - start_);
  t.line = line_;
  tokens_.push_back(std::move(t));
}

void Lexer::addToken(TokenType type, double number) {
  Token t;
  t.type = type;
  t.lexeme = source_.substr(start_, current_ - start_);
  t.line = line_;
  t.literal = number;
  tokens_.push_back(std::move(t));
}

bool Lexer::isAlpha(char c) { return std::isalpha((unsigned char)c) || c == '_'; }
bool Lexer::isDigit(char c) { return std::isdigit((unsigned char)c); }
bool Lexer::isAlphaNumeric(char c) { return isAlpha(c) || isDigit(c); }

std::vector<Token> Lexer::scanTokens() {
  while (!isAtEnd()) {
    start_ = current_;
    scanToken();
  }
  Token eof;
  eof.type = TokenType::EndOfFile;
  eof.lexeme = "";
  eof.line = line_;
  tokens_.push_back(eof);
  return tokens_;
}

void Lexer::scanToken() {
  char c = advance();
  switch (c) {
    case '(': addToken(TokenType::LParen); break;
    case ')': addToken(TokenType::RParen); break;
    case '{': addToken(TokenType::LBrace); break;
    case '}': addToken(TokenType::RBrace); break;
    case ';': addToken(TokenType::Semicolon); break;
    case ',': addToken(TokenType::Comma); break;
    case '+': addToken(TokenType::Plus); break;
    case '-': addToken(TokenType::Minus); break;
    case '*': addToken(TokenType::Star); break;

    case '/':
      if (match('/')) { // comment until end of line
        while (peek() != '\n' && !isAtEnd()) advance();
      } else {
        addToken(TokenType::Slash);
      }
      break;

    case '!': addToken(match('=') ? TokenType::BangEqual : TokenType::Bang); break;
    case '=': addToken(match('=') ? TokenType::EqualEqual : TokenType::Equal); break;
    case '<': addToken(match('=') ? TokenType::LessEqual : TokenType::Less); break;
    case '>': addToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater); break;

    case '&':
      if (match('&')) addToken(TokenType::AndAnd);
      else throw std::runtime_error("Unexpected '&' at line " + std::to_string(line_));
      break;

    case '|':
      if (match('|')) addToken(TokenType::OrOr);
      else throw std::runtime_error("Unexpected '|' at line " + std::to_string(line_));
      break;

    case ' ':
    case '\r':
    case '\t':
      break;

    case '\n':
      line_++;
      break;

    default:
      if (isDigit(c)) { number(); }
      else if (isAlpha(c)) { identifier(); }
      else {
        throw std::runtime_error(std::string("Unexpected character '") + c +
                                 "' at line " + std::to_string(line_));
      }
  }
}

void Lexer::number() {
  while (isDigit(peek())) advance();
  if (peek() == '.' && isDigit(peekNext())) {
    advance(); // consume '.'
    while (isDigit(peek())) advance();
  }
  const auto text = source_.substr(start_, current_ - start_);
  addToken(TokenType::Number, std::stod(text));
}

void Lexer::identifier() {
  while (isAlphaNumeric(peek())) advance();
  const auto text = source_.substr(start_, current_ - start_);

  static const std::unordered_map<std::string, TokenType> keywords = {
    {"let", TokenType::Let},
    {"if", TokenType::If},
    {"else", TokenType::Else},
    {"while", TokenType::While},
    {"print", TokenType::Print},
    {"true", TokenType::True},
    {"false", TokenType::False},
  };

  auto it = keywords.find(text);
  if (it != keywords.end()) {
    addToken(it->second);
  } else {
    addToken(TokenType::Identifier);
  }
}
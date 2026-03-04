
---

## 3) src/token.h

```cpp
#pragma once
#include <string>
#include <variant>

enum class TokenType {
  // Single-char
  LParen, RParen, LBrace, RBrace,
  Semicolon, Comma,
  Plus, Minus, Star, Slash,
  Bang, Equal, Less, Greater,

  // Two-char
  BangEqual, EqualEqual,
  LessEqual, GreaterEqual,
  AndAnd, OrOr,

  // Literals / identifiers
  Identifier, Number,

  // Keywords
  Let, If, Else, While, Print,
  True, False,

  EndOfFile
};

struct Token {
  TokenType type{};
  std::string lexeme{};
  int line = 1;
  std::variant<std::monostate, double> literal{};
};


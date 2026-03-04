# minilang — interpreter + compiler + VM (C++)

A small toy language implemented from scratch:
- Lexer → Parser (AST)
- AST Interpreter backend
- Bytecode Compiler backend
- Stack-based VM backend

## Build
```bash
cmake -S . -B build
cmake --build build -j
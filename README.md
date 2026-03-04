# # minilang — interpreter + compiler + VM (C++)

A small toy language implemented from scratch:
- Lexer → Parser (AST)
- AST Interpreter backend
- Bytecode Compiler backend
- Stack-based VM backend

## Build
```bash
cmake -S . -B build
cmake --build build -j

#Example ML 

let x = 0;
while (x < 5) {
  print x;
  x = x + 1;
}
if (x == 5) { print 123; } else { print 999; }

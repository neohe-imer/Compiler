#pragma once
#include "ast.h"
#include "bytecode.h"
#include <string>
#include <vector>
#include <unordered_map>

struct CompiledProgram {
  Chunk chunk;
  std::vector<std::string> namePool; // index -> variable name
};

class Compiler {
public:
  CompiledProgram compile(const std::vector<StmtPtr>& program);

private:
  Chunk chunk_;
  std::vector<std::string> namePool_;
  std::unordered_map<std::string, uint16_t> nameToIndex_;

  uint16_t nameIndex(const std::string& name);

  void emitConst(Value v);
  size_t emitJumpPlaceholder(Op op);
  void patchJump(size_t at, size_t target);

  void compileStmt(const Stmt* s);
  void compileExpr(const Expr* e);
};

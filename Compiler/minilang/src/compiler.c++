#include "compiler.h"
#include <stdexcept>
#include <limits>

uint16_t Compiler::nameIndex(const std::string& name) {
  auto it = nameToIndex_.find(name);
  if (it != nameToIndex_.end()) return it->second;
  if (namePool_.size() > std::numeric_limits<uint16_t>::max())
    throw std::runtime_error("Too many variable names (maximum 65536)");
  uint16_t idx = static_cast<uint16_t>(namePool_.size());
  namePool_.push_back(name);
  nameToIndex_[name] = idx;
  return idx;
}

void Compiler::emitConst(Value v) {
  if (chunk_.constants.size() > std::numeric_limits<uint16_t>::max())
    throw std::runtime_error("Too many constants (maximum 65536)");
  int idx = chunk_.addConst(std::move(v));
  chunk_.emit(Op::CONST);
  chunk_.emitU16((uint16_t)idx);
}

size_t Compiler::emitJumpPlaceholder(Op op) {
  chunk_.emit(op);
  const size_t at = chunk_.code.size();
  chunk_.emitU16(0);
  return at;
}

// Operands are signed 16-bit byte offsets relative to the end of the operand.
// Positions stay size_t: a large program is valid when each jump fits.
void Compiler::patchJump(size_t at, size_t target) {
  const auto offset = static_cast<int64_t>(target) - static_cast<int64_t>(at + 2);
  if (offset < std::numeric_limits<int16_t>::min() ||
      offset > std::numeric_limits<int16_t>::max())
    throw std::runtime_error("Jump exceeds signed 16-bit range");
  const auto encoded = static_cast<uint16_t>(offset);
  chunk_.code.at(at) = static_cast<uint8_t>(encoded & 0xff);
  chunk_.code.at(at + 1) = static_cast<uint8_t>(encoded >> 8);
}

CompiledProgram Compiler::compile(const std::vector<StmtPtr>& program) {
  chunk_ = Chunk{};
  namePool_.clear();
  nameToIndex_.clear();

  for (auto& s : program) compileStmt(s.get());

  chunk_.emit(Op::HALT);
  return CompiledProgram{ std::move(chunk_), std::move(namePool_) };
}

void Compiler::compileStmt(const Stmt* s) {
  if (auto* e = dynamic_cast<const ExprStmt*>(s)) {
    compileExpr(e->expr.get());
    chunk_.emit(Op::POP);
    return;
  }
  if (auto* p = dynamic_cast<const PrintStmt*>(s)) {
    compileExpr(p->expr.get());
    chunk_.emit(Op::PRINT);
    return;
  }
  if (auto* v = dynamic_cast<const VarDeclStmt*>(s)) {
    if (v->initializer) compileExpr(v->initializer.get());
    else emitConst(Value::num(0));
    // STORE nameIndex
    chunk_.emit(Op::STORE);
    chunk_.emitU16(nameIndex(v->name));
    return;
  }
  if (auto* b = dynamic_cast<const BlockStmt*>(s)) {
    for (auto& st : b->statements) compileStmt(st.get());
    return;
  }
  if (auto* i = dynamic_cast<const IfStmt*>(s)) {
    compileExpr(i->condition.get());
    // if false, jump over then
    auto jmpFalseAt = emitJumpPlaceholder(Op::JMP_IF_FALSE);

    compileStmt(i->thenBranch.get());

    // jump over else
    auto jmpEndAt = emitJumpPlaceholder(Op::JMP);

    patchJump(jmpFalseAt, chunk_.code.size());
    if (i->elseBranch) compileStmt(i->elseBranch.get());
    patchJump(jmpEndAt, chunk_.code.size());
    return;
  }

  if (auto* w = dynamic_cast<const WhileStmt*>(s)) {
    const size_t loopStart = chunk_.code.size();
    compileExpr(w->condition.get());
    const auto exitJump = emitJumpPlaceholder(Op::JMP_IF_FALSE);
    compileStmt(w->body.get());
    const auto backJump = emitJumpPlaceholder(Op::JMP);
    patchJump(backJump, loopStart);
    patchJump(exitJump, chunk_.code.size());
    return;
  }

  throw std::runtime_error("Unknown statement in compiler");
}

void Compiler::compileExpr(const Expr* e) {
  if (auto* l = dynamic_cast<const LiteralExpr*>(e)) {
    emitConst(l->value);
    return;
  }
  if (auto* v = dynamic_cast<const VariableExpr*>(e)) {
    chunk_.emit(Op::LOAD);
    chunk_.emitU16(nameIndex(v->name));
    return;
  }
  if (auto* a = dynamic_cast<const AssignExpr*>(e)) {
    compileExpr(a->value.get());
    chunk_.emit(Op::STORE);
    chunk_.emitU16(nameIndex(a->name));
    // store returns assigned value on stack in many VMs; we keep it by LOAD after STORE
    chunk_.emit(Op::LOAD);
    chunk_.emitU16(nameIndex(a->name));
    return;
  }
  if (auto* u = dynamic_cast<const UnaryExpr*>(e)) {
    compileExpr(u->right.get());
    switch (u->op.type) {
      case TokenType::Minus: chunk_.emit(Op::NEG); break;
      case TokenType::Bang:  chunk_.emit(Op::NOT); break;
      default: throw std::runtime_error("Unknown unary op in compiler");
    }
    return;
  }
  if (auto* b = dynamic_cast<const BinaryExpr*>(e)) {
    compileExpr(b->left.get());
    compileExpr(b->right.get());
    switch (b->op.type) {
      case TokenType::Plus: chunk_.emit(Op::ADD); break;
      case TokenType::Minus: chunk_.emit(Op::SUB); break;
      case TokenType::Star: chunk_.emit(Op::MUL); break;
      case TokenType::Slash: chunk_.emit(Op::DIV); break;

      case TokenType::EqualEqual: chunk_.emit(Op::EQ); break;
      case TokenType::BangEqual: chunk_.emit(Op::NE); break;
      case TokenType::Less: chunk_.emit(Op::LT); break;
      case TokenType::LessEqual: chunk_.emit(Op::LE); break;
      case TokenType::Greater: chunk_.emit(Op::GT); break;
      case TokenType::GreaterEqual: chunk_.emit(Op::GE); break;

      default: throw std::runtime_error("Unknown binary op in compiler");
    }
    return;
  }
  if (auto* lo = dynamic_cast<const LogicalExpr*>(e)) {
    compileExpr(lo->left.get());
    const auto falseJump = emitJumpPlaceholder(Op::JMP_IF_FALSE);
    if (lo->op.type == TokenType::AndAnd) {
      compileExpr(lo->right.get());
      // Normalize truthy numbers to a boolean, matching the interpreter.
      chunk_.emit(Op::NOT);
      chunk_.emit(Op::NOT);
      const auto endJump = emitJumpPlaceholder(Op::JMP);
      patchJump(falseJump, chunk_.code.size());
      emitConst(Value::boolean(false));
      patchJump(endJump, chunk_.code.size());
    } else if (lo->op.type == TokenType::OrOr) {
      emitConst(Value::boolean(true));
      const auto endJump = emitJumpPlaceholder(Op::JMP);
      patchJump(falseJump, chunk_.code.size());
      compileExpr(lo->right.get());
      chunk_.emit(Op::NOT);
      chunk_.emit(Op::NOT);
      patchJump(endJump, chunk_.code.size());
    } else {
      throw std::runtime_error("Unknown logical op in compiler");
    }
    return;
  }

  throw std::runtime_error("Unknown expression in compiler");
}

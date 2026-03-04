#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include "compiler.h"
#include "bytecode.h"
#include "value.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

// VM runner with:
// - signed JMP offsets
// - LOAD/STORE use namePool indices (uint16) resolved to string keys in globals
class VM2 {
public:
  void run(const CompiledProgram& prog) {
    stack_.clear();
    globals_.clear();

    const auto& code = prog.chunk.code;
    const auto& consts = prog.chunk.constants;
    const auto& pool = prog.namePool;

    size_t ip = 0;
    auto readU16 = [&](size_t& ipRef) -> uint16_t {
      uint16_t lo = code[ipRef++];
      uint16_t hi = code[ipRef++];
      return (uint16_t)(lo | (hi << 8));
    };
    auto readI16 = [&](size_t& ipRef) -> int16_t {
      return (int16_t)readU16(ipRef);
    };

    while (ip < code.size()) {
      Op op = (Op)code[ip++];

      switch (op) {
        case Op::CONST: {
          uint16_t idx = readU16(ip);
          push(consts[idx]);
        } break;

        case Op::LOAD: {
          uint16_t nameIdx = readU16(ip);
          const std::string& name = pool.at(nameIdx);
          auto it = globals_.find(name);
          if (it == globals_.end()) throw std::runtime_error("Undefined variable: " + name);
          push(it->second);
        } break;

        case Op::STORE: {
          uint16_t nameIdx = readU16(ip);
          const std::string& name = pool.at(nameIdx);
          auto v = pop();
          globals_[name] = v;
          // keep it simple: STORE does not push anything back
        } break;

        case Op::ADD: binNum([](double a,double b){return a+b;}); break;
        case Op::SUB: binNum([](double a,double b){return a-b;}); break;
        case Op::MUL: binNum([](double a,double b){return a*b;}); break;
        case Op::DIV: binNum([](double a,double b){return a/b;}); break;

        case Op::NEG: { auto a = pop(); push(Value::num(-a.asNum())); } break;

        case Op::EQ: cmpNum([](double a,double b){return a==b;}); break;
        case Op::NE: cmpNum([](double a,double b){return a!=b;}); break;
        case Op::LT: cmpNum([](double a,double b){return a<b;}); break;
        case Op::LE: cmpNum([](double a,double b){return a<=b;}); break;
        case Op::GT: cmpNum([](double a,double b){return a>b;}); break;
        case Op::GE: cmpNum([](double a,double b){return a>=b;}); break;

        case Op::NOT: { auto a = pop(); push(Value::boolean(!a.asBool())); } break;
        case Op::AND: { auto b = pop(); auto a = pop(); push(Value::boolean(a.asBool() && b.asBool())); } break;
        case Op::OR:  { auto b = pop(); auto a = pop(); push(Value::boolean(a.asBool() || b.asBool())); } break;

        case Op::JMP: {
          int16_t off = readI16(ip);
          ip = (size_t)((int64_t)ip + off);
        } break;

        case Op::JMP_IF_FALSE: {
          int16_t off = readI16(ip);
          auto cond = pop();
          if (!cond.asBool()) ip = (size_t)((int64_t)ip + off);
        } break;

        case Op::PRINT: {
          auto v = pop();
          std::cout << v.toString() << "\n";
        } break;

        case Op::POP: { (void)pop(); } break;
        case Op::HALT: return;
      }
    }
  }

private:
  std::vector<Value> stack_;
  std::unordered_map<std::string, Value> globals_;

  void push(Value v) { stack_.push_back(std::move(v)); }
  Value pop() {
    if (stack_.empty()) throw std::runtime_error("Stack underflow");
    auto v = stack_.back();
    stack_.pop_back();
    return v;
  }

  template <class F>
  void binNum(F f) {
    auto b = pop(); auto a = pop();
    push(Value::num(f(a.asNum(), b.asNum())));
  }

  template <class F>
  void cmpNum(F f) {
    auto b = pop(); auto a = pop();
    push(Value::boolean(f(a.asNum(), b.asNum())));
  }
};

// Patch compiler to support signed offsets for while + if jumps
static CompiledProgram compileWithWhile(const std::vector<StmtPtr>& program) {
  Compiler c;
  auto out = c.compile({}); // dummy to init (we'll re-implement compile here for while)
  // Instead of rewriting Compiler fully, implement a minimal inline compiler with while support:
  // For portfolio purposes, keep it straightforward.

  struct Ctx {
    Chunk chunk;
    std::vector<std::string> pool;
    std::unordered_map<std::string, uint16_t> map;
    uint16_t nameIndex(const std::string& n){
      auto it = map.find(n);
      if(it!=map.end()) return it->second;
      uint16_t idx=(uint16_t)pool.size();
      pool.push_back(n);
      map[n]=idx;
      return idx;
    }
    void emitConst(Value v){
      int idx=chunk.addConst(std::move(v));
      chunk.emit(Op::CONST); chunk.emitU16((uint16_t)idx);
    }
  } ctx;

  auto emitJumpPlaceholder = [&](Op op) -> uint16_t {
    ctx.chunk.emit(op);
    uint16_t at = (uint16_t)ctx.chunk.code.size();
    ctx.chunk.emitU16(0);
    return at;
  };
  auto patchJump = [&](uint16_t at, int16_t offsetFromEnd) {
    uint16_t u = (uint16_t)offsetFromEnd;
    ctx.chunk.code[at] = (uint8_t)(u & 0xFF);
    ctx.chunk.code[at+1] = (uint8_t)((u >> 8) & 0xFF);
  };

  std::function<void(const Expr*)> ce;
  std::function<void(const Stmt*)> cs;

  ce = [&](const Expr* e){
    if (auto* l = dynamic_cast<const LiteralExpr*>(e)) { ctx.emitConst(l->value); return; }
    if (auto* v = dynamic_cast<const VariableExpr*>(e)) {
      ctx.chunk.emit(Op::LOAD); ctx.chunk.emitU16(ctx.nameIndex(v->name)); return;
    }
    if (auto* a = dynamic_cast<const AssignExpr*>(e)) {
      ce(a->value.get());
      ctx.chunk.emit(Op::STORE); ctx.chunk.emitU16(ctx.nameIndex(a->name));
      ctx.chunk.emit(Op::LOAD); ctx.chunk.emitU16(ctx.nameIndex(a->name));
      return;
    }
    if (auto* u = dynamic_cast<const UnaryExpr*>(e)) {
      ce(u->right.get());
      if (u->op.type == TokenType::Minus) ctx.chunk.emit(Op::NEG);
      else if (u->op.type == TokenType::Bang) ctx.chunk.emit(Op::NOT);
      else throw std::runtime_error("bad unary");
      return;
    }
    if (auto* b = dynamic_cast<const BinaryExpr*>(e)) {
      ce(b->left.get()); ce(b->right.get());
      switch (b->op.type) {
        case TokenType::Plus: ctx.chunk.emit(Op::ADD); break;
        case TokenType::Minus: ctx.chunk.emit(Op::SUB); break;
        case TokenType::Star: ctx.chunk.emit(Op::MUL); break;
        case TokenType::Slash: ctx.chunk.emit(Op::DIV); break;
        case TokenType::EqualEqual: ctx.chunk.emit(Op::EQ); break;
        case TokenType::BangEqual: ctx.chunk.emit(Op::NE); break;
        case TokenType::Less: ctx.chunk.emit(Op::LT); break;
        case TokenType::LessEqual: ctx.chunk.emit(Op::LE); break;
        case TokenType::Greater: ctx.chunk.emit(Op::GT); break;
        case TokenType::GreaterEqual: ctx.chunk.emit(Op::GE); break;
        default: throw std::runtime_error("bad binary");
      }
      return;
    }
    if (auto* lo = dynamic_cast<const LogicalExpr*>(e)) {
      ce(lo->left.get()); ce(lo->right.get());
      if (lo->op.type == TokenType::AndAnd) ctx.chunk.emit(Op::AND);
      else if (lo->op.type == TokenType::OrOr) ctx.chunk.emit(Op::OR);
      else throw std::runtime_error("bad logical");
      return;
    }
    throw std::runtime_error("unknown expr");
  };

  cs = [&](const Stmt* s){
    if (auto* e = dynamic_cast<const ExprStmt*>(s)) { ce(e->expr.get()); ctx.chunk.emit(Op::POP); return; }
    if (auto* p = dynamic_cast<const PrintStmt*>(s)) { ce(p->expr.get()); ctx.chunk.emit(Op::PRINT); return; }
    if (auto* v = dynamic_cast<const VarDeclStmt*>(s)) {
      if (v->initializer) ce(v->initializer.get()); else ctx.emitConst(Value::num(0));
      ctx.chunk.emit(Op::STORE); ctx.chunk.emitU16(ctx.nameIndex(v->name));
      return;
    }
    if (auto* b = dynamic_cast<const BlockStmt*>(s)) { for (auto& st : b->statements) cs(st.get()); return; }
    if (auto* i = dynamic_cast<const IfStmt*>(s)) {
      ce(i->condition.get());
      uint16_t jf = emitJumpPlaceholder(Op::JMP_IF_FALSE);
      cs(i->thenBranch.get());
      uint16_t je = emitJumpPlaceholder(Op::JMP);

      // patch jf
      {
        int64_t after = (int64_t)(jf + 2);
        int64_t target = (int64_t)ctx.chunk.code.size();
        patchJump(jf, (int16_t)(target - after));
      }
      if (i->elseBranch) cs(i->elseBranch.get());

      // patch je
      {
        int64_t after = (int64_t)(je + 2);
        int64_t target = (int64_t)ctx.chunk.code.size();
        patchJump(je, (int16_t)(target - after));
      }
      return;
    }
    if (auto* w = dynamic_cast<const WhileStmt*>(s)) {
      int64_t loopStart = (int64_t)ctx.chunk.code.size();
      ce(w->condition.get());
      uint16_t exitJ = emitJumpPlaceholder(Op::JMP_IF_FALSE);
      cs(w->body.get());
      // jump back
      ctx.chunk.emit(Op::JMP);
      int64_t afterOp = (int64_t)ctx.chunk.code.size() + 2; // after operand
      ctx.chunk.emitU16(0);
      {
        int64_t target = loopStart;
        int16_t off = (int16_t)(target - afterOp);
        // patch the just-written u16 (located at end-2)
        uint16_t at = (uint16_t)(ctx.chunk.code.size() - 2);
        patchJump(at, off);
      }
      // patch exit
      {
        int64_t after = (int64_t)(exitJ + 2);
        int64_t target = (int64_t)ctx.chunk.code.size();
        patchJump(exitJ, (int16_t)(target - after));
      }
      return;
    }
    throw std::runtime_error("unknown stmt");
  };

  for (auto& s : program) cs(s.get());
  ctx.chunk.emit(Op::HALT);
  return CompiledProgram{ std::move(ctx.chunk), std::move(ctx.pool) };
}

static std::string readFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("Failed to open file: " + path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

int main(int argc, char** argv) {
  try {
    std::string mode = "--mode=interp";
    std::string path;

    for (int i = 1; i < argc; i++) {
      std::string a = argv[i];
      if (a.rfind("--mode=", 0) == 0) mode = a;
      else path = a;
    }

    if (path.empty()) {
      std::cerr << "Usage: minilang [--mode=interp|--mode=vm] file\n";
      return 2;
    }

    auto source = readFile(path);

    Lexer lexer(source);
    auto tokens = lexer.scanTokens();

    Parser parser(tokens);
    auto program = parser.parse();

    if (mode == "--mode=interp") {
      Interpreter interp;
      interp.run(program);
    } else if (mode == "--mode=vm") {
      auto compiled = compileWithWhile(program);
      VM2 vm;
      vm.run(compiled);
    } else {
      throw std::runtime_error("Unknown mode: " + mode);
    }

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}
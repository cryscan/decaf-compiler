/* File: ast_stmt.cc
 * -----------------
 * Implementation of statement node classes.
 */
#include "ast_stmt.h"
#include "ast_decl.h"
#include "ast_expr.h"
#include "ast_type.h"

extern CodeGenerator &codeGen;
extern FnDecl *arrayLengthFn;

Program::Program(List<Decl *> *d) {
  Assert(d != NULL);
  (decls = d)->SetParentAll(this);
  CollectSymbols(decls);
}

void Program::Check() {
  /* You can use your pp3 semantic analysis or leave it out if
   * you want to avoid the clutter.  We won't test pp4 against
   * semantically-invalid programs.
   */
  arrayLengthFn = new FnDecl(new Identifier(yylloc, "length"), Type::intType,
                             new List<VarDecl *>);

  bool hasMain = false;
  for (auto decl : decls->Get()) {
    decl->Check();
    if (auto fn = dynamic_cast<FnDecl *>(decl)) {
      fn->SetLabel();
      if (strcmp(fn->GetName(), "main") == 0)
        hasMain = true;
    }
  }

  if (!hasMain)
    ReportError::NoMainFound();
}

void Program::Emit() {
  /* pp4: here is where the code generation is kicked off.
   *      The general idea is perform a tree traversal of the
   *      entire program, generating instructions as you go.
   *      Each node can have its own way of translating itself,
   *      which makes for a great use of inheritance and
   *      polymorphism in the node classes.
   */
  for (auto decl : decls->Get())
    decl->Emit();

  codeGen.PostProcess();
  // codeGen.DoFinalCodeGen();
}

StmtBlock::StmtBlock(List<VarDecl *> *d, List<Stmt *> *s) {
  Assert(d != NULL && s != NULL);
  (decls = d)->SetParentAll(this);
  (stmts = s)->SetParentAll(this);
  CollectSymbols(decls);
}

void StmtBlock::Check() {
  for (auto decl : decls->Get())
    decl->Check();
  for (auto stmt : stmts->Get())
    stmt->Check();
}

void StmtBlock::Emit() {
  for (auto decl : decls->Get())
    decl->Emit();
  for (auto stmt : stmts->Get())
    stmt->Emit();
}

ConditionalStmt::ConditionalStmt(Expr *t, Stmt *b) {
  Assert(t != NULL && b != NULL);
  (test = t)->SetParent(this);
  (body = b)->SetParent(this);
}

void ConditionalStmt::Check() {
  if (test->Eval() != Type::boolType)
    ReportError::TestNotBoolean(test);
  body->Check();
}

void LoopStmt::Emit() {
  labelBefore = CodeGenerator::Instance().NewLabel();
  labelAfter = CodeGenerator::Instance().NewLabel();
}

ForStmt::ForStmt(Expr *i, Expr *t, Expr *s, Stmt *b) : LoopStmt(t, b) {
  Assert(i != NULL && t != NULL && s != NULL && b != NULL);
  (init = i)->SetParent(this);
  (step = s)->SetParent(this);
}

void ForStmt::Check() {
  if (test->Eval() != Type::boolType)
    ReportError::TestNotBoolean(test);
  init->Eval();
  step->Eval();
  body->Check();
}

void ForStmt::Emit() {
  LoopStmt::Emit();
  init->Emit();

  codeGen.GenLabel(labelBefore);
  test->Emit();
  auto val = test->GetValue();
  Assert(val);
  codeGen.GenIfZ(val, labelAfter);

  body->Emit();
  step->Emit();
  codeGen.GenGoto(labelBefore);
  codeGen.GenLabel(labelAfter);
}

void WhileStmt::Emit() {
  LoopStmt::Emit();
  codeGen.GenLabel(labelBefore);
  test->Emit();
  auto val = test->GetValue();
  Assert(val);
  codeGen.GenIfZ(val, labelAfter);

  body->Emit();
  codeGen.GenGoto(labelBefore);
  codeGen.GenLabel(labelAfter);
}

IfStmt::IfStmt(Expr *t, Stmt *tb, Stmt *eb) : ConditionalStmt(t, tb) {
  Assert(t != NULL && tb != NULL); // else can be NULL
  elseBody = eb;
  if (elseBody)
    elseBody->SetParent(this);
}

void IfStmt::Check() {
  ConditionalStmt::Check();
  if (elseBody)
    elseBody->Check();
}

void IfStmt::Emit() {
  test->Emit();
  auto val = test->GetValue();
  Assert(val);

  labelAfter = CodeGenerator::Instance().NewLabel();
  if (elseBody) {
    auto labelElse = codeGen.NewLabel();
    codeGen.GenIfZ(val, labelElse);
    body->Emit();
    codeGen.GenGoto(labelAfter);

    codeGen.GenLabel(labelElse);
    elseBody->Emit();
  } else {
    codeGen.GenIfZ(val, labelAfter);
    body->Emit();
  }

  codeGen.GenLabel(labelAfter);
}

void BreakStmt::Check() {
  loop = FindParentByType<LoopStmt *>();
  if (!loop)
    ReportError::BreakOutsideLoop(this);
}

void BreakStmt::Emit() {
  Assert(loop);
  auto labelAfter = loop->GetLabelAfter();
  codeGen.GenGoto(labelAfter);
}

ReturnStmt::ReturnStmt(yyltype loc, Expr *e) : Stmt(loc) {
  Assert(e != NULL);
  (expr = e)->SetParent(this);
}

void ReturnStmt::Check() {
  auto fn = FindParentByType<FnDecl *>();
  Assert(fn);
  auto given = expr->Eval();
  auto expected = fn->GetReturnType();
  if (!given->IsConvertableTo(expected))
    ReportError::ReturnMismatch(this, given, expected);
}

void ReturnStmt::Emit() {
  expr->Emit();
  auto val = expr->GetValue();
  codeGen.GenReturn(val);
}

PrintStmt::PrintStmt(List<Expr *> *a) {
  Assert(a != NULL);
  (args = a)->SetParentAll(this);
}

void PrintStmt::Check() {
  for (int i = 0; i < args->NumElements(); ++i) {
    auto arg = args->Nth(i);
    auto type = arg->Eval();
    if (type != Type::stringType && type != Type::intType &&
        type != Type::boolType)
      ReportError::PrintArgMismatch(arg, i + 1, type);
  }
}

void PrintStmt::Emit() {
  for (auto expr : args->Get()) {
    expr->Emit();
    auto type = expr->GetType();
    auto val = expr->GetValue();
    if (type == Type::stringType)
      codeGen.GenBuiltInCall(PrintString, val);
    else if (type == Type::intType)
      codeGen.GenBuiltInCall(PrintInt, val);
    else if (type == Type::boolType)
      codeGen.GenBuiltInCall(PrintBool, val);
  }
}
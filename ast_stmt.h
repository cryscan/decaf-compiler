/* File: ast_stmt.h
 * ----------------
 * The Stmt class and its subclasses are used to represent
 * statements in the parse tree.  For each statment in the
 * language (for, if, return, etc.) there is a corresponding
 * node class for that construct.
 *
 * pp4: You will need to extend the Stmt classes to implement
 * code generation for statements.
 */

#ifndef _H_ast_stmt
#define _H_ast_stmt

#include "ast.h"
#include "list.h"

class Decl;
class VarDecl;
class Expr;

class Program : public Node {
protected:
  List<Decl *> *decls;

public:
  Program(List<Decl *> *declList);
  void Check();
  void Emit();
};

class Stmt : public Node {
public:
  Stmt() : Node() {}
  Stmt(yyltype loc) : Node(loc) {}
  virtual void Check() = 0;
};

class StmtBlock : public Stmt {
protected:
  List<VarDecl *> *decls;
  List<Stmt *> *stmts;

public:
  StmtBlock(List<VarDecl *> *variableDeclarations, List<Stmt *> *statements);
  void Check();
  void Emit();
};

class ConditionalStmt : public Stmt {
protected:
  Expr *test;
  Stmt *body;
  char *labelAfter;

public:
  ConditionalStmt(Expr *testExpr, Stmt *body);
  void Check();

  char *GetLabelAfter() const { return labelAfter; }
};

class LoopStmt : public ConditionalStmt {
protected:
  char *labelBefore;

public:
  LoopStmt(Expr *testExpr, Stmt *body)
      : ConditionalStmt(testExpr, body), labelBefore{nullptr} {}
  void Emit();

  char *GetLabelBefore() const { return labelBefore; }
};

class ForStmt : public LoopStmt {
protected:
  Expr *init, *step;

public:
  ForStmt(Expr *init, Expr *test, Expr *step, Stmt *body);
  void Check();
  void Emit();
};

class WhileStmt : public LoopStmt {
public:
  WhileStmt(Expr *test, Stmt *body) : LoopStmt(test, body) {}
  void Check() { ConditionalStmt::Check(); }
  void Emit();
};

class IfStmt : public ConditionalStmt {
protected:
  Stmt *elseBody;

public:
  IfStmt(Expr *test, Stmt *thenBody, Stmt *elseBody);
  void Check();
  void Emit();
};

class BreakStmt : public Stmt {
protected:
  LoopStmt *loop;

public:
  BreakStmt(yyltype loc) : Stmt(loc), loop{nullptr} {}
  void Check();
  void Emit();
};

class ReturnStmt : public Stmt {
protected:
  Expr *expr;

public:
  ReturnStmt(yyltype loc, Expr *expr);
  void Check();
  void Emit();
};

class PrintStmt : public Stmt {
protected:
  List<Expr *> *args;

public:
  PrintStmt(List<Expr *> *arguments);
  void Check();
  void Emit();
};

#endif

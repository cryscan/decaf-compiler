/* File: ast_decl.h
 * ----------------
 * In our parse tree, Decl nodes are used to represent and
 * manage declarations. There are 4 subclasses of the base class,
 * specialized for declarations of variables, functions, classes,
 * and interfaces.
 *
 * pp4: You will need to extend the Decl classes to implement
 * code generation for declarations.
 */

#ifndef _H_ast_decl
#define _H_ast_decl

#include "ast.h"
#include "ast_type.h"
#include "codegen.h"
#include "list.h"

class Identifier;
class Stmt;

class Decl : public Node {
protected:
  Identifier *id;

public:
  Decl(Identifier *name);
  friend std::ostream &operator<<(std::ostream &out, Decl *d) {
    return out << d->id;
  }
  virtual void Check() = 0;

  char *GetName() const { return id->GetName(); }
};

class VarDecl : public Decl {
protected:
  Type *type;
  Location *valLoc;
  int offset;

public:
  VarDecl(Identifier *name, Type *type);
  void Check();
  void Emit();

  Type *GetType() const { return type; }
  Location *GetValue() const { return valLoc; }

  void SetOffset(int offset) { this->offset = offset; };
  int GetOffset() const { return offset; }
};

class ClassDecl : public Decl {
protected:
  List<Decl *> *members;
  NamedType *extends;
  List<NamedType *> *implements;
  NamedType *type;
  int size;
  List<FnDecl *> *methods;

public:
  ClassDecl(Identifier *name, NamedType *extends, List<NamedType *> *implements,
            List<Decl *> *members);
  void Check();
  void Emit();

  Decl *FindSymbolInClass(const char *name) const;
  void CollectVar();
  void CollectFn();

  Type *GetType() const { return type; }
  int GetSize() const { return size; }
  ClassDecl *GetBase() const;
  bool IsDerivedFrom(ClassDecl *other) const;
};

class InterfaceDecl : public Decl {
protected:
  List<Decl *> *members;

public:
  InterfaceDecl(Identifier *name, List<Decl *> *members);
  void Check();
};

class FnDecl : public Decl {
protected:
  List<VarDecl *> *formals;
  List<Type *> *formalTypes;
  Type *returnType;
  Stmt *body;
  char *label;
  int offset;

public:
  FnDecl(Identifier *name, Type *returnType, List<VarDecl *> *formals);
  void SetFunctionBody(Stmt *b);
  void Check();
  void Emit();

  Type *GetReturnType() const { return returnType; }
  List<Type *> *GetFormalTypes() const { return formalTypes; }

  void SetLabel(const char *className = nullptr);
  char *GetLabel() const { return label; }

  void SetOffset(int offset) { this->offset = offset; }
  int GetOffset() const { return offset; }

  bool IsMatched(const FnDecl *other) const;
};

#endif

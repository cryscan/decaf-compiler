/* File: ast_type.h
 * ----------------
 * In our parse tree, Type nodes are used to represent and
 * store type information. The base Type class is used
 * for built-in types, the NamedType for classes and interfaces,
 * and the ArrayType for arrays of other types.
 *
 * pp4: You will need to extend the Type classes to implement
 * code generation for types.
 */

#ifndef _H_ast_type
#define _H_ast_type

#include "ast.h"
#include "list.h"
#include <iostream>

class ClassDecl;
class FnDecl;

class Type : public Node {
protected:
  char *typeName;

public:
  static Type *intType, *doubleType, *boolType, *voidType, *nullType,
      *stringType, *errorType;

  Type(yyltype loc) : Node(loc) {}
  Type(const char *str);

  char *GetName() const { return typeName; }

  virtual void PrintToStream(std::ostream &out) { out << typeName; }
  friend std::ostream &operator<<(std::ostream &out, Type *t) {
    t->PrintToStream(out);
    return out;
  }
  virtual bool IsEquivalentTo(const Type *other) const { return this == other; }
  virtual bool IsConvertableTo(const Type *other) const;
};

class NamedType : public Type {
protected:
  Identifier *id;

public:
  NamedType(Identifier *i);
  char *GetName() const { return id->GetName(); }
  Identifier *GetIdentifier() const { return id; }

  void PrintToStream(std::ostream &out) { out << id; }
  bool IsEquivalentTo(const Type *other) const {
    return strcmp(GetName(), other->GetName()) == 0;
  }
  bool IsConvertableTo(const Type *other) const;
  ClassDecl *FindClassDecl() const;
};

class ArrayType : public Type {
protected:
  Type *elemType;

public:
  ArrayType(yyltype loc, Type *elemType);
  Type *GetElemType() const { return elemType; }

  void PrintToStream(std::ostream &out) { out << elemType << "[]"; }
  bool IsEquivalentTo(const Type *other) const {
    return strcmp(GetName(), other->GetName()) == 0;
  }
};

#endif

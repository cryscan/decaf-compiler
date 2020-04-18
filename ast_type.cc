/* File: ast_type.cc
 * -----------------
 * Implementation of type node classes.
 */
#include "ast_type.h"
#include "ast_decl.h"
#include "ast_stmt.h"
#include <string.h>

/* Class constants
 * ---------------
 * These are public constants for the built-in base types (int, double, etc.)
 * They can be accessed with the syntax Type::intType. This allows you to
 * directly access them and share the built-in types where needed rather that
 * creates lots of copies.
 */

Type *Type::intType = new Type("int");
Type *Type::doubleType = new Type("double");
Type *Type::voidType = new Type("void");
Type *Type::boolType = new Type("bool");
Type *Type::nullType = new Type("null");
Type *Type::stringType = new Type("string");
Type *Type::errorType = new Type("error");

Type::Type(const char *n) {
  Assert(n);
  typeName = strdup(n);
}

bool Type::IsConvertableTo(const Type *other) const {
  auto namedType = dynamic_cast<const NamedType *>(other);
  if (this == nullType && namedType) {
    return true;
  }
  return IsEquivalentTo(other) || this == errorType || other == errorType;
}

NamedType::NamedType(Identifier *i) : Type(*i->GetLocation()) {
  Assert(i != NULL);
  (id = i)->SetParent(this);
}

bool NamedType::IsConvertableTo(const Type *other) const {
  if (other == Type::errorType)
    return true;
  if (auto mine = FindClassDecl()) {
    if (IsEquivalentTo(other))
      return true;
    if (auto named = dynamic_cast<const NamedType *>(other)) {
      if (auto others = named->FindClassDecl()) {
        return mine->IsDerivedFrom(others);
      }
    }
  }
  return false;
}

ClassDecl *NamedType::FindClassDecl() const {
  auto program = FindParentByType<Program *>();
  auto decl = program->FindSymbol(GetName());
  if (auto cls = dynamic_cast<ClassDecl *>(decl))
    return cls;
  return nullptr;
}

ArrayType::ArrayType(yyltype loc, Type *et) : Type(loc) {
  Assert(et != NULL);
  (elemType = et)->SetParent(this);

  auto elemTypeName = elemType->GetName();
  typeName = new char[strlen(elemTypeName) + 3];
  sprintf(typeName, "%s[]", elemTypeName);
}

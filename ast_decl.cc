/* File: ast_decl.cc
 * -----------------
 * Implementation of Decl node classes.
 */
#include "ast_decl.h"
#include "ast_stmt.h"
#include "ast_type.h"
#include "codegen.h"
#include <algorithm>

extern CodeGenerator &codeGen;
FnDecl *arrayLengthFn = nullptr;

Decl::Decl(Identifier *n) : Node(*n->GetLocation()) {
  Assert(n != NULL);
  (id = n)->SetParent(this);
}

VarDecl::VarDecl(Identifier *n, Type *t) : Decl(n), valLoc{nullptr}, offset{0} {
  Assert(n != NULL && t != NULL);
  (type = t)->SetParent(this);
}

void VarDecl::Check() {
  Assert(parent);
  if (auto cls = dynamic_cast<ClassDecl *>(parent)) {
    if (auto base = cls->GetBase()) {
      auto prev = base->FindSymbolInClass(GetName());
      if (prev != nullptr && prev != this)
        ReportError::DeclConflict(this, prev);
    }
  }
}

void VarDecl::Emit() {
  Assert(parent);
  auto name = GetName();
  if (dynamic_cast<Program *>(parent))
    valLoc = codeGen.GenGlobalVar(name);
  else if (dynamic_cast<FnDecl *>(parent))
    valLoc = codeGen.GenParamVar(name);
  else if (dynamic_cast<StmtBlock *>(parent))
    valLoc = codeGen.GenLocalVar(name);
}

ClassDecl::ClassDecl(Identifier *n, NamedType *ex, List<NamedType *> *imp,
                     List<Decl *> *m)
    : Decl(n), size{0}, methods{nullptr} {
  // extends can be NULL, impl & mem may be empty lists but cannot be NULL
  Assert(n != NULL && imp != NULL && m != NULL);
  extends = ex;
  if (extends)
    extends->SetParent(this);
  (implements = imp)->SetParentAll(this);
  (members = m)->SetParentAll(this);
  (type = new NamedType(id))->SetParent(this);
  CollectSymbols(members);
}

void ClassDecl::Check() {
  if (extends) {
    auto base = GetBase();
    if (!base) {
      ReportError::IdentifierNotDeclared(extends->GetIdentifier(),
                                         reasonT::LookingForClass);
      extends = nullptr;
    }
    if (base->IsDerivedFrom(this))
      extends = nullptr;
  }

  for (auto member : members->Get()) {
    member->Check();
    if (auto fn = dynamic_cast<FnDecl *>(member))
      fn->SetLabel(GetName());
  }

  CollectVar();
  CollectFn();
}

void ClassDecl::Emit() {
  for (auto member : members->Get())
    member->Emit();

  auto methodLabels = new List<const char *>;
  for (auto fn : methods->Get())
    methodLabels->Append(fn->GetLabel());
  codeGen.GenVTable(GetName(), methodLabels);
}

Decl *ClassDecl::FindSymbolInClass(const char *name) const {
  Assert(symbols);
  if (auto ret = symbols->Lookup(name))
    return ret;
  if (extends) {
    auto base = GetBase();
    Assert(base);
    return base->FindSymbolInClass(name);
  }
  return nullptr;
}

void ClassDecl::CollectFn() {
  if (methods != nullptr)
    return;
  methods = new List<FnDecl *>;
  if (extends) {
    auto base = GetBase();
    base->CollectFn();
    for (auto method : base->methods->Get())
      methods->Append(method);

    int size = methods->NumElements();
    for (auto member : members->Get())
      if (auto fn = dynamic_cast<FnDecl *>(member)) {
        bool overriden = false;
        for (auto &method : methods->Get()) {
          if (fn->IsMatched(method)) {
            fn->SetOffset(method->GetOffset());
            method = fn;
            overriden = true;
          }
        }
        if (!overriden) {
          int offset = methods->NumElements() * codeGen.VarSize;
          fn->SetOffset(offset);
          methods->Append(fn);
        }
      }
  } else {
    for (auto member : members->Get())
      if (auto fn = dynamic_cast<FnDecl *>(member)) {
        int offset = methods->NumElements() * codeGen.VarSize;
        fn->SetOffset(offset);
        methods->Append(fn);
      }
  }
}

void ClassDecl::CollectVar() {
  if (size > 0)
    return;
  if (extends) {
    auto base = GetBase();
    base->CollectVar();
    size += base->size;
  } else
    size = codeGen.VarSize; // for vtable

  for (auto member : members->Get())
    if (auto var = dynamic_cast<VarDecl *>(member)) {
      var->SetOffset(size);
      size += codeGen.VarSize;
    }
}

ClassDecl *ClassDecl::GetBase() const {
  if (!extends)
    return nullptr;
  if (auto base = extends->FindClassDecl())
    return base;
  return nullptr;
}

bool ClassDecl::IsDerivedFrom(ClassDecl *other) const {
  if (other == this)
    return true;
  if (auto base = GetBase())
    return base->IsDerivedFrom(other);
  return false;
}

InterfaceDecl::InterfaceDecl(Identifier *n, List<Decl *> *m) : Decl(n) {
  Assert(n != NULL && m != NULL);
  (members = m)->SetParentAll(this);
  CollectSymbols(members);
}

void InterfaceDecl::Check() {
  for (auto member : members->Get())
    member->Check();
}

FnDecl::FnDecl(Identifier *n, Type *r, List<VarDecl *> *d) : Decl(n) {
  Assert(n != NULL && r != NULL && d != NULL);
  (returnType = r)->SetParent(this);
  (formals = d)->SetParentAll(this);
  body = NULL;

  formalTypes = new List<Type *>;
  for (auto var : formals->Get())
    formalTypes->Append(var->GetType());

  CollectSymbols(formals);
}

void FnDecl::SetFunctionBody(Stmt *b) { (body = b)->SetParent(this); }

void FnDecl::Check() {
  if (auto namedType = dynamic_cast<NamedType *>(returnType)) {
    auto decl = parent->FindSymbolInParents(namedType->GetName());
    auto classDecl = dynamic_cast<ClassDecl *>(decl);
    if (!classDecl)
      ReportError::IdentifierNotDeclared(namedType->GetIdentifier(),
                                         reasonT::LookingForClass);
  }
  if (auto decl = FindParentByType<ClassDecl *>()) {
    if (auto base = decl->GetBase()) {
      if (auto prev = base->FindSymbolInClass(GetName())) {
        if (auto fn = dynamic_cast<FnDecl *>(prev)) {
          if (!IsMatched(fn)) {
            ReportError::OverrideMismatch(this);
          }
        } else {
          ReportError::DeclConflict(this, prev);
        }
      }
    }
  }
  for (auto var : formals->Get())
    var->Check();
  if (body)
    body->Check();
}

void FnDecl::Emit() {
  Assert(parent);
  codeGen.GenLabel(label);
  auto beginFunc = codeGen.GenBeginFunc();

  if (dynamic_cast<ClassDecl *>(parent))
    codeGen.GenParamVar("this");
  for (auto var : formals->Get())
    var->Emit();
  if (body)
    body->Emit();

  beginFunc->SetFrameSize(codeGen.GetFrameSize());
  codeGen.GenEndFunc();
}

void FnDecl::SetLabel(const char *className) {
  char temp[1024];
  auto name = GetName();
  if (className == nullptr) {
    if (strcmp(name, "main") == 0)
      label = strdup(name);
    else {
      sprintf(temp, "_%s", name);
      label = strdup(temp);
    }
  } else {
    sprintf(temp, "_%s.%s", className, name);
    label = strdup(temp);
  }
}

bool FnDecl::IsMatched(const FnDecl *other) const {
  if (strcmp(GetName(), other->GetName()) != 0)
    return false;
  if (!returnType->IsEquivalentTo(other->returnType))
    return false;
  if (formals->NumElements() != other->formals->NumElements())
    return false;
  for (int i = 0; i < formals->NumElements(); ++i) {
    auto t1 = formals->Nth(i)->GetType();
    auto t2 = other->formals->Nth(i)->GetType();
    if (!(t1->IsEquivalentTo(t2)))
      return false;
  }
  return true;
}
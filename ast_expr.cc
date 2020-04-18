/* File: ast_expr.cc
 * -----------------
 * Implementation of expression node classes.
 */
#include "ast_expr.h"
#include "ast_decl.h"
#include "ast_type.h"
#include <string.h>

extern CodeGenerator &codeGen;
extern FnDecl *arrayLengthFn;

IntConstant::IntConstant(yyltype loc, int val) : Expr(loc), value{val} {}

void IntConstant::Emit() { valLoc = codeGen.GenLoadConstant(value); }

DoubleConstant::DoubleConstant(yyltype loc, double val)
    : Expr(loc), value{val} {}

BoolConstant::BoolConstant(yyltype loc, bool val) : Expr(loc), value{val} {}

void BoolConstant::Emit() { valLoc = codeGen.GenLoadConstant(value); }

StringConstant::StringConstant(yyltype loc, const char *val) : Expr(loc) {
  Assert(val != NULL);
  value = strdup(val);
}

void StringConstant::Emit() { valLoc = codeGen.GenLoadConstant(value); }

void NullConstant::Emit() { valLoc = codeGen.GenLoadConstant(0); }

Operator::Operator(yyltype loc, const char *tok) : Node(loc) {
  Assert(tok != NULL);
  strncpy(tokenString, tok, sizeof(tokenString));
}

CompoundExpr::CompoundExpr(Expr *l, Operator *o, Expr *r)
    : Expr(Join(l->GetLocation(), r->GetLocation())) {
  Assert(l != NULL && o != NULL && r != NULL);
  (op = o)->SetParent(this);
  (left = l)->SetParent(this);
  (right = r)->SetParent(this);
}

CompoundExpr::CompoundExpr(Operator *o, Expr *r)
    : Expr(Join(o->GetLocation(), r->GetLocation())) {
  Assert(o != NULL && r != NULL);
  left = NULL;
  (op = o)->SetParent(this);
  (right = r)->SetParent(this);
}

Type *ArithmeticExpr::Eval() {
  auto rhs = right->Eval();
  if (left) {
    auto lhs = left->Eval();
    if (lhs == Type::intType && rhs == Type::intType)
      type = Type::intType;
    else if (lhs == Type::doubleType && rhs == Type::doubleType)
      type = Type::doubleType;
    else if (lhs == Type::errorType || rhs == Type::errorType)
      type = Type::errorType;
    else {
      ReportError::IncompatibleOperands(op, lhs, rhs);
      type = Type::errorType;
    }
  } else {
    if (rhs == Type::intType || rhs == Type::doubleType ||
        rhs == Type::errorType)
      type = rhs;
    else {
      ReportError::IncompatibleOperand(op, rhs);
      type = Type::errorType;
    }
  }
  return type;
}

void ArithmeticExpr::Emit() {
  right->Emit();
  Location *lhs = nullptr;
  auto rhs = right->GetValue();
  if (left) {
    left->Emit();
    lhs = left->GetValue();
  } else
    lhs = codeGen.GenLoadConstant(0);
  valLoc = codeGen.GenBinaryOp(op->GetName(), lhs, rhs);
}

Type *RelationalExpr::Eval() {
  auto lhs = left->Eval();
  auto rhs = right->Eval();
  if (lhs == Type::intType && rhs == Type::intType)
    type = Type::boolType;
  else if (lhs == Type::doubleType && rhs == Type::doubleType)
    type = Type::boolType;
  else if (lhs == Type::errorType || rhs == Type::errorType)
    type = Type::errorType;
  else {
    ReportError::IncompatibleOperands(op, lhs, rhs);
    type = Type::errorType;
  }
  return type;
}

void RelationalExpr::Emit() {
  left->Emit();
  right->Emit();

  auto tok = op->GetName();
  auto lhs = left->GetValue();
  auto rhs = right->GetValue();
  if (strcmp("<", tok) == 0)
    valLoc = codeGen.GenBinaryOp("<", lhs, rhs);
  else if (strcmp(">", tok) == 0)
    valLoc = codeGen.GenBinaryOp("<", rhs, lhs);
  else if (strcmp("<=", tok) == 0) {
    auto lt = codeGen.GenBinaryOp("<", lhs, rhs);
    auto eq = codeGen.GenBinaryOp("==", lhs, rhs);
    valLoc = codeGen.GenBinaryOp("||", lt, eq);
  } else if (strcmp(">=", tok) == 0) {
    auto gt = codeGen.GenBinaryOp("<", rhs, lhs);
    auto eq = codeGen.GenBinaryOp("==", lhs, rhs);
    valLoc = codeGen.GenBinaryOp("||", gt, eq);
  } else
    Assert(false);
}

Type *EqualityExpr::Eval() {
  auto lhs = left->Eval();
  auto rhs = right->Eval();
  if (lhs == Type::errorType || rhs == Type::errorType)
    type = Type::errorType;
  else if (lhs->IsConvertableTo(rhs) || rhs->IsConvertableTo(lhs))
    type = Type::boolType;
  else {
    ReportError::IncompatibleOperands(op, lhs, rhs);
    type = Type::errorType;
  }
  return type;
}

void EqualityExpr::Emit() {
  left->Emit();
  right->Emit();
  auto tok = op->GetName();
  auto lhs = left->GetValue();
  auto rhs = right->GetValue();
  Location *eq = nullptr;

  if (left->GetType() == Type::stringType)
    eq = codeGen.GenBuiltInCall(StringEqual, lhs, rhs);
  else
    eq = codeGen.GenBinaryOp("==", lhs, rhs);

  if (strcmp(tok, "==") == 0)
    valLoc = eq;
  else if (strcmp(tok, "!=") == 0) {
    auto zero = codeGen.GenLoadConstant(0);
    valLoc = codeGen.GenBinaryOp("==", eq, zero);
  }
}

Type *LogicalExpr::Eval() {
  auto rhs = right->Eval();
  if (left) {
    auto lhs = left->Eval();
    if (lhs == Type::boolType && rhs == Type::boolType)
      type = Type::boolType;
    else if (lhs == Type::errorType || rhs == Type::errorType)
      type = Type::errorType;
    else {
      ReportError::IncompatibleOperands(op, lhs, rhs);
      type = Type::errorType;
    }
  } else {
    if (rhs == Type::boolType || rhs == Type::errorType)
      type = rhs;
    else {
      ReportError::IncompatibleOperand(op, rhs);
      type = Type::errorType;
    }
  }
  return type;
}

void LogicalExpr::Emit() {
  right->Emit();
  Location *lhs = nullptr;
  auto rhs = right->GetValue();
  if (left) {
    left->Emit();
    lhs = left->GetValue();
    valLoc = codeGen.GenBinaryOp(op->GetName(), lhs, rhs);
  } else {
    Assert(strcmp(op->GetName(), "!") == 0);
    lhs = codeGen.GenLoadConstant(0);
    valLoc = codeGen.GenBinaryOp("==", lhs, rhs);
  }
}

Type *AssignExpr::Eval() {
  auto lhs = left->Eval();
  auto rhs = right->Eval();
  if (lhs == Type::errorType || rhs == Type::errorType)
    type = Type::errorType;
  else if (!rhs->IsConvertableTo(lhs)) {
    ReportError::IncompatibleOperands(op, lhs, rhs);
    type = Type::errorType;
  } else
    type = rhs;
  return type;
}

void AssignExpr::Emit() {
  left->Emit();
  right->Emit();
  auto lv = dynamic_cast<LValue *>(left);
  auto rhs = right->GetValue();
  lv->Assign(rhs);
}

Location *AssignExpr::GetValue() const {
  if (valLoc)
    return valLoc;
  else
    return left->GetValue();
}

Type *This::Eval() {
  auto cls = FindParentByType<ClassDecl *>();
  if (!cls) {
    ReportError::ThisOutsideClassScope(this);
    return type = Type::errorType;
  }
  return type = cls->GetType();
}

void This::Emit() { valLoc = codeGen.GenThis(); }

Location *LValue::GetValue() const {
  if (valLoc)
    return valLoc;
  else {
    Assert(addr);
    return codeGen.GenLoad(addr, offset);
  }
}

void LValue::Assign(Location *src) const {
  if (valLoc)
    codeGen.GenAssign(valLoc, src);
  else {
    Assert(addr);
    Assert(src);
    codeGen.GenStore(addr, src, offset);
  }
}

ArrayAccess::ArrayAccess(yyltype loc, Expr *b, Expr *s) : LValue(loc) {
  (base = b)->SetParent(this);
  (subscript = s)->SetParent(this);
}

Type *ArrayAccess::Eval() {
  type = base->Eval();
  if (type != Type::errorType) {
    if (auto arrayType = dynamic_cast<ArrayType *>(type))
      type = arrayType->GetElemType();
    else {
      ReportError::BracketsOnNonArray(base);
      type = Type::errorType;
    }
  }

  auto subType = subscript->Eval();
  if (subType != Type::errorType && subType != Type::intType)
    ReportError::SubscriptNotInteger(subscript);

  return type;
}

void ArrayAccess::Emit() {
  base->Emit();
  subscript->Emit();

  auto array = base->GetValue();
  auto index = subscript->GetValue();
  auto length = codeGen.GenLoad(array, -codeGen.VarSize);

  // runtime check for index bound
  auto labelHalt = codeGen.NewLabel();
  auto labelAfter = codeGen.NewLabel();
  auto negOne = codeGen.GenLoadConstant(-1);
  auto lower = codeGen.GenBinaryOp("<", negOne, index);
  auto upper = codeGen.GenBinaryOp("<", index, length);
  auto test = codeGen.GenBinaryOp("&&", lower, upper);
  codeGen.GenIfZ(test, labelHalt);

  auto varSize = codeGen.GenLoadConstant(codeGen.VarSize);
  auto offset = codeGen.GenBinaryOp("*", index, varSize);
  addr = codeGen.GenBinaryOp("+", array, offset);
  codeGen.GenGoto(labelAfter);

  codeGen.GenLabel(labelHalt);
  auto message = codeGen.GenLoadConstant(err_arr_out_of_bounds);
  codeGen.GenBuiltInCall(PrintString, message);
  codeGen.GenBuiltInCall(Halt);
  codeGen.GenLabel(labelAfter);
}

FieldAccess::FieldAccess(Expr *b, Identifier *f)
    : LValue(b ? Join(b->GetLocation(), f->GetLocation()) : *f->GetLocation()),
      var{nullptr} {
  Assert(f != NULL); // b can be be NULL (just means no explicit base)
  base = b;
  if (base)
    base->SetParent(this);
  (field = f)->SetParent(this);
}

Type *FieldAccess::Eval() {
  ClassDecl *cls = nullptr;
  if (base) {
    type = base->Eval();
    if (type == Type::errorType)
      return type;
    if (auto namedType = dynamic_cast<NamedType *>(type)) {
      cls = namedType->FindClassDecl();
      auto parent = FindParentByType<ClassDecl *>();
      if (!(parent->IsDerivedFrom(cls)))
        ReportError::InaccessibleField(field, type);
    } else {
      ReportError::FieldNotFoundInBase(field, type);
      return type = Type::errorType;
    }
    if (cls)
      if (auto decl = cls->FindSymbolInClass(field->GetName()))
        var = dynamic_cast<VarDecl *>(decl);
    if (!var) {
      ReportError::FieldNotFoundInBase(field, type);
      return type = Type::errorType;
    }
  } else {
    if (auto decl = parent->FindSymbolInClass(field->GetName()))
      var = dynamic_cast<VarDecl *>(decl);
    else if (auto decl = parent->FindSymbolInParents(field->GetName()))
      var = dynamic_cast<VarDecl *>(decl);
    if (!var) {
      ReportError::IdentifierNotDeclared(field, reasonT::LookingForVariable);
      return type = Type::errorType;
    }
  }
  return type = var->GetType();
}

void FieldAccess::Emit() {
  Assert(var);
  if (auto loc = var->GetValue())
    valLoc = loc;
  else {
    offset = var->GetOffset();
    if (base) {
      base->Emit();
      addr = base->GetValue(); // pointer to the base object
    } else
      addr = codeGen.GenThis();
  }
}

Call::Call(yyltype loc, Expr *b, Identifier *f, List<Expr *> *a)
    : Expr(loc), fn{nullptr} {
  Assert(f != NULL &&
         a != NULL); // b can be be NULL (just means no explicit base)
  base = b;
  if (base)
    base->SetParent(this);
  (field = f)->SetParent(this);
  (actuals = a)->SetParentAll(this);
}

Type *Call::Eval() {
  ClassDecl *cls = nullptr;
  if (base) {
    type = base->Eval();
    if (type == Type::errorType)
      return type;
    if (auto namedType = dynamic_cast<NamedType *>(type))
      cls = namedType->FindClassDecl();
    else if (auto arrayType = dynamic_cast<ArrayType *>(type))
      fn = arrayLengthFn;
    else {
      ReportError::FieldNotFoundInBase(field, type);
      return type = Type::errorType;
    }
    if (cls) {
      if (auto decl = cls->FindSymbolInClass(field->GetName()))
        fn = dynamic_cast<FnDecl *>(decl);
    }
    if (!fn) {
      ReportError::FieldNotFoundInBase(field, type);
      return type = Type::errorType;
    }
  } else {
    cls = FindParentByType<ClassDecl *>();
    if (cls)
      if (auto decl = cls->FindSymbolInClass(field->GetName()))
        fn = dynamic_cast<FnDecl *>(decl);
    if (!fn)
      if (auto decl = parent->FindSymbolInParents(field->GetName()))
        fn = dynamic_cast<FnDecl *>(decl);
    if (!fn) {
      ReportError::IdentifierNotDeclared(field, reasonT::LookingForFunction);
      return type = Type::errorType;
    }
  }

  auto types = fn->GetFormalTypes();
  if (types->NumElements() != actuals->NumElements()) {
    ReportError::NumArgsMismatch(field, types->NumElements(),
                                 actuals->NumElements());
  } else
    for (int i = 0; i < types->NumElements(); ++i) {
      auto actual = actuals->Nth(i);
      auto type = actual->Eval();
      auto expected = types->Nth(i);
      if (!type->IsConvertableTo(expected))
        ReportError::ArgMismatch(actual, i + 1, type, expected);
    }

  return type = fn->GetReturnType();
}

void Call::Emit() {
  Assert(fn);

  auto fnParent = fn->GetParent();
  bool hasReturn = (fn->GetReturnType() != Type::voidType);

  if (dynamic_cast<Program *>(fnParent)) {
    List<Location *> params;
    for (auto actual : actuals->Get()) {
      actual->Emit();
      auto param = actual->GetValue();
      params.InsertAt(param, 0);
    }
    for (auto param : params.Get())
      codeGen.GenPushParam(param);
    auto label = fn->GetLabel();
    valLoc = codeGen.GenLCall(label, hasReturn);
    codeGen.GenPopParams(params.NumElements() * codeGen.VarSize);
  } else if (dynamic_cast<ClassDecl *>(fnParent)) {
    Location *addr = nullptr;
    Location *object = nullptr;
    if (base) {
      base->Emit();
      object = base->GetValue();
    } else
      object = codeGen.GenThis();
    auto vtable = codeGen.GenLoad(object);
    int offset = fn->GetOffset();
    addr = codeGen.GenLoad(vtable, offset);

    List<Location *> params;
    params.Append(object); // the first param is 'this'
    for (auto actual : actuals->Get()) {
      actual->Emit();
      auto param = actual->GetValue();
      params.InsertAt(param, 0);
    }
    for (auto param : params.Get())
      codeGen.GenPushParam(param);
    valLoc = codeGen.GenACall(addr, hasReturn);
    codeGen.GenPopParams(params.NumElements() * codeGen.VarSize);
  } else if (fn == arrayLengthFn) {
    Assert(base);
    base->Emit();
    auto array = base->GetValue();
    valLoc = codeGen.GenLoad(array, -codeGen.VarSize);
  }
}

NewExpr::NewExpr(yyltype loc, NamedType *c) : Expr(loc) {
  Assert(c != NULL);
  (cType = c)->SetParent(this);
}

Type *NewExpr::Eval() {
  if (cType->FindClassDecl())
    type = cType;
  else {
    ReportError::IdentifierNotDeclared(cType->GetIdentifier(),
                                       reasonT::LookingForClass);
    type = Type::errorType;
  }
  return type;
}

void NewExpr::Emit() {
  auto &codeGen = CodeGenerator::Instance();
  auto cls = cType->FindClassDecl();
  Assert(cls);

  auto size = codeGen.GenLoadConstant(cls->GetSize());
  valLoc = codeGen.GenBuiltInCall(Alloc, size);

  auto vtable = codeGen.GenLoadLabel(cls->GetName());
  codeGen.GenStore(valLoc, vtable);
}

NewArrayExpr::NewArrayExpr(yyltype loc, Expr *sz, Type *et) : Expr(loc) {
  Assert(sz != NULL && et != NULL);
  (size = sz)->SetParent(this);
  (elemType = et)->SetParent(this);
}

Type *NewArrayExpr::Eval() {
  if (size->Eval() != Type::intType)
    ReportError::NewArraySizeNotInteger(size);
  if (auto namedType = dynamic_cast<NamedType *>(elemType))
    if (!namedType->FindClassDecl()) {
      ReportError::IdentifierNotDeclared(namedType->GetIdentifier(),
                                         reasonT::LookingForType);
      return type = Type::errorType;
    }
  return type = new ArrayType(yylloc, elemType);
}

void NewArrayExpr::Emit() {
  size->Emit();
  auto length = size->GetValue();

  auto one = codeGen.GenLoadConstant(1);
  auto labelAfter = codeGen.NewLabel();
  auto test = codeGen.GenBinaryOp("<", length, one);
  codeGen.GenIfZ(test, labelAfter);
  auto message = codeGen.GenLoadConstant(err_arr_bad_size);
  codeGen.GenBuiltInCall(PrintString, message);
  codeGen.GenBuiltInCall(Halt);
  codeGen.GenLabel(labelAfter);

  auto varSize = codeGen.GenLoadConstant(codeGen.VarSize);
  auto arraySize = codeGen.GenBinaryOp("*", varSize, length);
  auto totalSize = codeGen.GenBinaryOp("+", varSize, arraySize);
  auto addr = codeGen.GenBuiltInCall(Alloc, totalSize);
  codeGen.GenStore(addr, length);
  valLoc = codeGen.GenBinaryOp("+", addr, varSize);
}

void ReadIntegerExpr::Emit() { valLoc = codeGen.GenBuiltInCall(ReadInteger); }

void ReadLineExpr::Emit() { valLoc = codeGen.GenBuiltInCall(ReadLine); }
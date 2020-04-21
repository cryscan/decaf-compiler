/* File: codegen.cc
 * ----------------
 * Implementation for the CodeGenerator class. The methods don't do anything
 * too fancy, mostly just create objects of the various Tac instruction
 * classes and append them to the list.
 */

#include "codegen.h"
#include "mips.h"
#include "tac.h"
#include <algorithm>
#include <string.h>

auto &codeGen = CodeGenerator::Instance();

CodeGenerator::CodeGenerator()
    : globalCounter{0}, paramCounter{0}, localCounter{0} {
  code = new List<Instruction *>;
  labels = new std::map<std::string, Label *>;
}

CodeGenerator &CodeGenerator::Instance() {
  static CodeGenerator instance;
  return instance;
}

int CodeGenerator::GetFrameSize() { return VarSize * localCounter; }

char *CodeGenerator::NewLabel() {
  static int nextLabelNum = 0;
  char temp[10];
  sprintf(temp, "_L%d", nextLabelNum++);
  return strdup(temp);
}

Location *CodeGenerator::GenTempVar() {
  static int nextTempNum;
  char temp[10];
  sprintf(temp, "_tmp%d", nextTempNum++);

  /* pp4: need to create variable in proper location
     in stack frame for use as temporary. Until you
     do that, the assert below will always fail to remind
     you this needs to be implemented  */
  int offset = OffsetToFirstLocal - VarSize * localCounter++;

  Location *result = new Location(fpRelative, offset, temp);
  Assert(result != NULL);
  return result;
}

Location *CodeGenerator::GenLocalVar(const char *name) {
  int offset = OffsetToFirstLocal - VarSize * localCounter++;
  return new Location(fpRelative, offset, name);
}

Location *CodeGenerator::GenGlobalVar(const char *name) {
  int offset = OffsetToFirstGlobal + VarSize * globalCounter++;
  return new Location(gpRelative, offset, name);
}

Location *CodeGenerator::GenParamVar(const char *name) {
  int offset = OffsetToFirstParam + VarSize * paramCounter++;
  return new Location(fpRelative, offset, name);
}

Location *CodeGenerator::GenThis() {
  return new Location(fpRelative, OffsetToFirstParam, "this");
}

Location *CodeGenerator::GenLoadConstant(int value) {
  Location *result = GenTempVar();
  code->Append(new LoadConstant(result, value));
  return result;
}

Location *CodeGenerator::GenLoadConstant(const char *s) {
  Location *result = GenTempVar();
  code->Append(new LoadStringConstant(result, s));
  return result;
}

Location *CodeGenerator::GenLoadLabel(const char *label) {
  Location *result = GenTempVar();
  code->Append(new LoadLabel(result, label));
  return result;
}

void CodeGenerator::GenAssign(Location *dst, Location *src) {
  code->Append(new Assign(dst, src));
}

Location *CodeGenerator::GenLoad(Location *ref, int offset) {
  Location *result = GenTempVar();
  code->Append(new Load(result, ref, offset));
  return result;
}

void CodeGenerator::GenStore(Location *dst, Location *src, int offset) {
  code->Append(new Store(dst, src, offset));
}

Location *CodeGenerator::GenBinaryOp(const char *opName, Location *op1,
                                     Location *op2) {
  Location *result = GenTempVar();
  code->Append(new BinaryOp(BinaryOp::OpCodeForName(opName), result, op1, op2));
  return result;
}

void CodeGenerator::GenLabel(const char *label) {
  code->Append(new Label(label));
}

void CodeGenerator::GenIfZ(Location *test, const char *label) {
  code->Append(new IfZ(test, label));
}

void CodeGenerator::GenGoto(const char *label) {
  code->Append(new Goto(label));
}

void CodeGenerator::GenReturn(Location *val) { code->Append(new Return(val)); }

BeginFunc *CodeGenerator::GenBeginFunc() {
  BeginFunc *result = new BeginFunc;
  code->Append(result);
  return result;
}

void CodeGenerator::GenEndFunc() {
  code->Append(new EndFunc());
  localCounter = 0;
  paramCounter = 0;
}

void CodeGenerator::GenPushParam(Location *param) {
  code->Append(new PushParam(param));
}

void CodeGenerator::GenPopParams(int numBytesOfParams) {
  Assert(numBytesOfParams >= 0 &&
         numBytesOfParams % VarSize == 0); // sanity check
  if (numBytesOfParams > 0)
    code->Append(new PopParams(numBytesOfParams));
}

Location *CodeGenerator::GenLCall(const char *label, bool fnHasReturnValue) {
  Location *result = fnHasReturnValue ? GenTempVar() : NULL;
  code->Append(new LCall(label, result));
  return result;
}

Location *CodeGenerator::GenACall(Location *fnAddr, bool fnHasReturnValue) {
  Location *result = fnHasReturnValue ? GenTempVar() : NULL;
  code->Append(new ACall(fnAddr, result));
  return result;
}

static struct _builtin {
  const char *label;
  int numArgs;
  bool hasReturn;
} builtins[] = {{"_Alloc", 1, true},       {"_ReadLine", 0, true},
                {"_ReadInteger", 0, true}, {"_StringEqual", 2, true},
                {"_PrintInt", 1, false},   {"_PrintString", 1, false},
                {"_PrintBool", 1, false},  {"_Halt", 0, false}};

Location *CodeGenerator::GenBuiltInCall(BuiltIn bn, Location *arg1,
                                        Location *arg2) {
  Assert(bn >= 0 && bn < NumBuiltIns);
  struct _builtin *b = &builtins[bn];
  Location *result = NULL;

  if (b->hasReturn)
    result = GenTempVar();
  // verify appropriate number of non-NULL arguments given
  Assert((b->numArgs == 0 && !arg1 && !arg2) ||
         (b->numArgs == 1 && arg1 && !arg2) ||
         (b->numArgs == 2 && arg1 && arg2));
  if (arg2)
    code->Append(new PushParam(arg2));
  if (arg1)
    code->Append(new PushParam(arg1));
  code->Append(new LCall(b->label, result));
  GenPopParams(VarSize * b->numArgs);
  return result;
}

void CodeGenerator::GenVTable(const char *className,
                              List<const char *> *methodLabels) {
  code->Append(new VTable(className, methodLabels));
}

void CodeGenerator::DoFinalCodeGen() {
  if (IsDebugOn("tac")) { // if debug don't translate to mips, just print Tac
    for (int i = 0; i < code->NumElements(); i++)
      code->Nth(i)->Print();
  } else {
    Mips mips;
    mips.EmitPreamble();
    for (int i = 0; i < code->NumElements(); i++)
      code->Nth(i)->Emit(&mips);
  }
}

void CodeGenerator::CollectLabels() {
  for (auto inst : code->Get())
    if (auto label = dynamic_cast<Label *>(inst))
      labels->emplace(label->GetLabel(), label);
}

void Goto::AddSucc(Instruction *) {
  auto labels = codeGen.GetLabels();
  auto inst = labels->at(label);
  succ->Append(inst);
}

void IfZ::AddSucc(Instruction *next) {
  Instruction::AddSucc(next);

  auto labels = codeGen.GetLabels();
  auto inst = labels->at(label);
  succ->Append(inst);
}

void CodeGenerator::BuildControlFlow(int begin, int end) {
  for (int i = begin; i < end; ++i) {
    auto inst = code->Nth(i);
    auto next = code->Nth(i + 1);
    inst->AddSucc(next);
  }
}

void CodeGenerator::LiveAnalyze(int begin, int end) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (int i = begin; i < end; ++i) {
      auto inst = code->Nth(i);
      if (inst->UpdateLiveVar())
        changed = true;
    }
  }
}

void CodeGenerator::AllocRegister(int begin, int end) {
  Graph<Location *> graph;
  LocationSet varSet;

  for (int i = begin; i < end; ++i) {
    auto inst = code->Nth(i);
    LocationSet interf, kill = inst->Kill(), gen = inst->Gen(),
                        out = inst->GetOut();
    std::set_union(kill.begin(), kill.end(), out.begin(), out.end(),
                   std::inserter(interf, interf.begin()));

    for (auto u : interf)
      for (auto v : interf)
        graph.AddEdge(u, v);

    std::set_union(kill.begin(), kill.end(), gen.begin(), gen.end(),
                   std::inserter(varSet, varSet.begin()));
  }

  graph.KColor(Mips::NumGeneralPurposeRegs);
  auto color = graph.GetColor();

  for (auto var : varSet) {
    auto index = color[var];
    if (index > 0) {
      auto reg = Mips::Register((int)Mips::t0 + index - 1);
      var->SetRegister(reg);
    }
  }
}

void CodeGenerator::DoCodeGen(int begin, int end) {
  if (IsDebugOn("tac")) { // if debug don't translate to mips, just print Tac
    for (int i = begin; i < end; ++i)
      code->Nth(i)->Print();
  } else {
    Mips mips;
    mips.EmitPreamble();
    for (int i = begin; i < end; ++i)
      code->Nth(i)->Emit(&mips);
  }
}

void CodeGenerator::Process() {
  CollectLabels();

  int begin = 0, end = 0;
  for (int i = 0; i < code->NumElements(); ++i) {
    auto inst = code->Nth(i);
    if (dynamic_cast<BeginFunc *>(inst)) {
      begin = i;

      DoCodeGen(end, begin);
    } else if (dynamic_cast<EndFunc *>(inst)) {
      end = i;
      BuildControlFlow(begin, end);
      LiveAnalyze(begin, end);
      AllocRegister(begin, end);

      DoCodeGen(begin, end);
    }
  }
  DoCodeGen(end, code->NumElements());
}
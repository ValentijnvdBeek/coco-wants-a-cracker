#include <iostream>

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
Constant *computeConstantResult(const BasicBlock::iterator I,
                                const int64_t ValA, const int64_t ValB) {
  switch (I->getOpcode()) {
    case Instruction::Add:
      return ConstantInt::get(I->getType(), ValA + ValB);
    case Instruction::Sub:
      return ConstantInt::get(I->getType(), ValA - ValB);
    case Instruction::Mul:
      return ConstantInt::get(I->getType(), ValA * ValB);
    case Instruction::Shl:
      return ConstantInt::get(I->getType(), ValA << ValB);
    case Instruction::LShr:
      return ConstantInt::get(I->getType(), static_cast<uint64_t>(ValA) >>
                                                static_cast<uint64_t>(ValB));
    case Instruction::AShr:
      return ConstantInt::get(I->getType(), ValA >> ValB);
    case Instruction::And:
      return ConstantInt::get(I->getType(), ValA & ValB);
    case Instruction::Or:
      return ConstantInt::get(I->getType(), ValA | ValB);
    case Instruction::Xor:
      return ConstantInt::get(I->getType(), ValA ^ ValB);
    case Instruction::UDiv:
      if (ValB == 0) return nullptr;  // Avoid division by zero
      return ConstantInt::get(I->getType(), static_cast<uint64_t>(ValA) /
                                                static_cast<uint64_t>(ValB));
    case Instruction::SDiv:
      if (ValB == 0) return nullptr;  // Avoid division by zero
      return ConstantInt::get(I->getType(), ValA / ValB);
    case Instruction::URem:
      if (ValB == 0) return nullptr;  // Avoid division by zero
      return ConstantInt::get(I->getType(), static_cast<uint64_t>(ValA) %
                                                static_cast<uint64_t>(ValB));
    case Instruction::SRem:
      if (ValB == 0) return nullptr;  // Avoid division by zero
      return ConstantInt::get(I->getType(), ValA % ValB);
    default:
      return nullptr;
  }
}

bool precomputeConstantValues(BasicBlock::iterator I) {
  const Value *ArgA = I->getOperand(0);
  const Value *ArgB = I->getOperand(1);

  // We require both sides to be constants
  if (!isa<Constant>(ArgA) || !isa<Constant>(ArgB)) return false;

  // Extract the values from both sides and calculate the result
  const ConstantInt *ConstA = dyn_cast<ConstantInt>(ArgA);
  const ConstantInt *ConstB = dyn_cast<ConstantInt>(ArgB);
  const int64_t ValA = ConstA->getSExtValue();
  const int64_t ValB = ConstB->getSExtValue();
  Constant *ReplaceValue = computeConstantResult(I, ValA, ValB);

  // If we can't find it, we print an error and return
  if (!ReplaceValue) {
    fprintf(stderr, "[InstCombiner] Cannot find opcode '%s'\n",
            I->getOpcodeName());
    return false;
  }
  fprintf(stderr, "[InstCombiner] Replacing with %ld %s %ld = %ld\n", ValA,
          I->getOpcodeName(), ValB,
          dyn_cast<ConstantInt>(ReplaceValue)->getSExtValue());
  I->replaceAllUsesWith(ReplaceValue);
  return true;
}

bool isIdentityOperation(const unsigned opcode, const uint64_t Value) {
  switch (opcode) {
    case Instruction::Add:
    case Instruction::Sub:
    case Instruction::Shl:
    case Instruction::LShr:
    case Instruction::AShr:
    case Instruction::Or:
    case Instruction::Xor:
      return Value == 0;

    case Instruction::Mul:
    case Instruction::UDiv:
    case Instruction::SDiv:
      return Value == 1;

    case Instruction::And:
    case Instruction::URem:
      return Value == UINT64_MAX;
    case Instruction::SRem:
      return Value == INT64_MAX;
    default:
      return false;
  }
}

bool replaceIdentityOperations(BasicBlock::iterator I) {
  Value *ArgA = I->getOperand(0);
  Value *ArgB = I->getOperand(1);

  // We assume that the constants have already been calculated
  // and that therefore only one side can be constant. We do
  // require one to be constant. With two variables, you can
  // never be sure whether it is an identity or not.
  const bool IsLeftContant = isa<Constant>(ArgA);
  const bool IsRightConstant = isa<Constant>(ArgB);

  if (!IsLeftContant && !IsRightConstant) return false;

  // division, substract and modulo are not reversible operations.
  // So we need to make sure that we only operate on the right
  // hand value, otherwise we generate junk IR.
  const unsigned Opcode = I->getOpcode();
  if ((Opcode == Instruction::UDiv || Opcode == Instruction::SDiv ||
       Opcode == Instruction::Sub || Opcode == Instruction::URem ||
       Opcode == Instruction::SRem) &&
      !IsRightConstant) {
    return false;
  }

  // Find the constant and variable sides
  Value *VarSide = IsLeftContant ? ArgB : ArgA;
  Value *ConstSide = IsLeftContant ? ArgA : ArgB;
  const int64_t Value = dyn_cast<ConstantInt>(ConstSide)->getSExtValue();

  // If they are in identity operation, replace them with the value
  if (!isIdentityOperation(I->getOpcode(), Value)) return false;

  fprintf(stderr, "[InstCombiner] Replacing $val %s %ld => $val\n",
          I->getOpcodeName(), Value);
  I->replaceAllUsesWith(VarSide);
  return true;
}

bool replaceWithConstant(BasicBlock::iterator I) {
  Value *ArgA = I->getOperand(0);
  Value *ArgB = I->getOperand(1);

  // We assume that the constants have already been calculated
  // and that therefore only one side can be constant. We do
  // require one to be constant. With two variables, you can
  // never be sure whether it is an identity or not.
  const bool IsLeftContant = isa<Constant>(ArgA);
  const bool IsRightConstant = isa<Constant>(ArgB);
  if (!IsLeftContant && !IsRightConstant) return false;

  // division, substract and modulo are not reversible operations.
  // So we need to make sure that we only operate on the right
  // hand value, otherwise we generate junk IR.
  const unsigned Opcode = I->getOpcode();
  if ((Opcode == Instruction::UDiv || Opcode == Instruction::SDiv) &&
      IsLeftContant) {
    const int64_t ConstValue = dyn_cast<ConstantInt>(ArgA)->getSExtValue();
    if (ConstValue != 0) return false;

    fputs("[InstCombiner] Replacing 0 / $var with = 0\n", stderr);
    I->replaceAllUsesWith(ArgA);
    return true;
  }

  if ((Opcode == Instruction::URem || Opcode == Instruction::SRem) &&
      IsRightConstant) {
    const int64_t ConstValue = dyn_cast<ConstantInt>(ArgB)->getSExtValue();
    if (ConstValue != 1) return false;

    fputs("[InstCombiner] Replacing $var % 1 = 0\n", stderr);
    Value *ZeroVal = ConstantInt::get(I->getType(), 0);
    I->replaceAllUsesWith(ZeroVal);
    return true;
  }

  if (Opcode == Instruction::Mul || Opcode == Instruction::And) {
    Value *VarSide = IsLeftContant ? ArgB : ArgA;
    Value *ConstSide = IsLeftContant ? ArgA : ArgB;
    const int64_t Value = dyn_cast<ConstantInt>(ConstSide)->getSExtValue();
    if (Value != 0) return false;

    fputs("[InstCombiner] Replacing $var * 0 = 0\n", stderr);
    I->replaceAllUsesWith(ConstSide);
    return true;
  }

  return false;
}

bool isConstant(GlobalVariable* GV) {
  for (User *U : GV->users()) {
    if (auto *SI = dyn_cast<StoreInst>(U)) {
      return false;
    }

    if (auto *GEP = dyn_cast<GetElementPtrInst>(U)) {
      return false;
    }

    if (auto *GEP = dyn_cast<CallInst>(U)) {
      return false;
    }
  }
  return true;
}

Instruction* constantPropagation(BasicBlock::iterator I) {
  // Check if a computation comes from a global variable that fixed during the programing life cycle.
  Instruction &Inst = *I;

  Value *ArgA = Inst.getOperand(0);
  Value *ArgB = Inst.getOperand(1);

  if (auto Load = dyn_cast<LoadInst>(ArgA)) {
    // propagate(Load);
    auto ptr = Load->getPointerOperand();

    if (auto GV = dyn_cast<GlobalVariable>(ptr)) {
      if (isConstant(GV)) {
        auto initializer = GV->getInitializer();

        if (auto *CInt = dyn_cast<ConstantInt>(initializer)) {
          int64_t Val = CInt->getSExtValue();
          auto load_ty = Load->getType();
          Value *NewInstr = ConstantInt::get(load_ty, Val);

          Load->replaceAllUsesWith(NewInstr);
          return Load;
        }
      }
    }
  }

  if (auto Load = dyn_cast<LoadInst>(ArgB)) {
    auto ptr = Load->getPointerOperand();

    if (auto GV = dyn_cast<GlobalVariable>(ptr)) {
      if (isConstant(GV)) {
        auto initializer = GV->getInitializer();

        if (auto *CInt = dyn_cast<ConstantInt>(initializer)) {
          int64_t Val = CInt->getSExtValue();
          auto load_ty = Load->getType();
          Value *NewInstr = ConstantInt::get(load_ty, Val);

          Load->replaceAllUsesWith(NewInstr);
          return Load;
        }
      }
    }
  }

  return NULL;
}

bool reductionDivToConst(IRBuilder<> &Builder, BasicBlock::iterator It) {
  Instruction &Inst = *It;
  Value *ArgA = Inst.getOperand(0);
  Value *ArgB = Inst.getOperand(1);

  unsigned Opcode = Inst.getOpcode();
  if (Opcode != Instruction::UDiv && Opcode != Instruction::SDiv) {
    return false;
  }

  // Check if ArgA and ArgB are both LoadInst from the same pointer
  auto *lhsLoad = dyn_cast<LoadInst>(ArgA);
  auto *rhsLoad = dyn_cast<LoadInst>(ArgB);
  if (!lhsLoad || !rhsLoad) {
    return false;
  }

  // Must be loading from the same pointer
  if (lhsLoad->getPointerOperand() != rhsLoad->getPointerOperand()) {
    return false;
  }

  // TODO (Zhiyang): Optionally ensure there's no store in between these loads
  //       that modifies the pointer, so the loaded value hasn't changed.

  // Example: a / a => 1
  Value *NewInstr = ConstantInt::get(lhsLoad->getType(), 1);

  Inst.replaceAllUsesWith(NewInstr);
  return true;
}


bool reductionDivToShift(IRBuilder<> &Builder, BasicBlock::iterator I) {
  Value *ArgA = I->getOperand(0);
  Value *ArgB = I->getOperand(1);

  // We assume that the constants have already been calculated
  // and that therefore only one side can be constant. We do
  // require one to be constant. With two variables, you can
  // never be sure whether it is an identity or not.
  const bool IsLeftContant = isa<Constant>(ArgA);
  const bool IsRightConstant = isa<Constant>(ArgB);
  const unsigned Opcode = I->getOpcode();
  if (!IsRightConstant) return false;

  if (Opcode != Instruction::Mul && Opcode != Instruction::UDiv &&
      Opcode != Instruction::SDiv && Opcode != Instruction::URem &&
      Opcode != Instruction::SRem) {
    return false;
  }

  // multiplication by a power of two can be replace by a shift to left
  const int64_t ConstValue = dyn_cast<ConstantInt>(ArgB)->getSExtValue();
  const int64_t PowerOfTwo = log2l(ConstValue);

  if (!isPowerOf2_64(ConstValue)) return false;

  // We need an else-branch here because the builder inserts
  // it directly into the source stream, so using a default
  // value causes to add useless instructions
  Value *NewInstr;
  if (Opcode == Instruction::Mul) {
    NewInstr = Builder.CreateShl(ArgA, PowerOfTwo);
    fprintf(stderr, "[InstCombiner] Replacing x %s %ld with x << %ld\n",
            I->getOpcodeName(), PowerOfTwo);

  } else if (Opcode == Instruction::UDiv || Opcode == Instruction::SDiv) {
    // Firstly check the cases like : a / a = 1.
    auto lhs_inst = dyn_cast<LoadInst>(ArgA);
    auto rhs_inst = dyn_cast<LoadInst>(ArgA);

    NewInstr = Builder.CreateAShr(ArgA, PowerOfTwo);
    fprintf(stderr, "[InstCombiner] Replacing x %s %ld with x >> %ld\n",
            I->getOpcodeName(), ConstValue, PowerOfTwo);

  } else if (Opcode == Instruction::URem || Opcode == Instruction::SRem) {
    Value *PowerMinusOne = ConstantInt::get(I->getType(), ConstValue - 1);
    NewInstr = Builder.CreateAnd(ArgA, PowerMinusOne);
    fprintf(stderr, "[InstCombiner] Replacing x %s %ld with x ^ %ld\n",
            I->getOpcodeName(), PowerOfTwo - 1);

  } else {
    llvm_unreachable("[InstCombiner] Invalid opcode replacement!");
  }

  I->replaceAllUsesWith(NewInstr);
  return true;
}

struct CocoInstCombine : PassInfoMixin<CocoInstCombine> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    std::list<Instruction *> deleteInstructionQueue;
    bool FixedPoint = false;
    while (!FixedPoint) {
      for (Function::iterator BB = F.begin(); BB != F.end(); BB++) {
        for (BasicBlock::iterator I = BB->begin(); I != BB->end(); I++) {
          switch (I->getOpcode()) {
            case Instruction::Add:
            case Instruction::Sub:
            case Instruction::Mul:
            case Instruction::Shl:
            case Instruction::LShr:
            case Instruction::AShr:
            case Instruction::And:
            case Instruction::Or:
            case Instruction::Xor:
            case Instruction::UDiv:
            case Instruction::SDiv:
            case Instruction::URem:
            case Instruction::SRem: {
              Instruction *Instr = dyn_cast<Instruction>(I);

              Instruction * RedundentLoad = constantPropagation(I);
              if (RedundentLoad) {
                deleteInstructionQueue.push_back(RedundentLoad);
                continue;
              }

              if (precomputeConstantValues(I)) {
                deleteInstructionQueue.push_back(Instr);
                continue;
              }

              if (replaceIdentityOperations(I)) {
                deleteInstructionQueue.push_back(Instr);
                continue;
              }

              if (replaceWithConstant(I)) {
                deleteInstructionQueue.push_back(Instr);
                continue;
              }

              IRBuilder<> Builder(Instr);
              if (reductionDivToConst(Builder, I)) {
                deleteInstructionQueue.push_back(Instr);
                continue;
              }

              if (reductionDivToShift(Builder, I)) {
                deleteInstructionQueue.push_back(Instr);
                continue;
              }

              break;
            }
          }
        }

        if (deleteInstructionQueue.size() == 0) FixedPoint = true;

        for (Instruction *Instr : deleteInstructionQueue) {
          // Check if the instruction is already removed
          if (!Instr) continue;

          // Check if the parent is already removed
          // This check is not done by isSafeToRemove()
          // for some reason...
          if (!Instr->getParent()) continue;

          if (Instr->isSafeToRemove()) Instr->removeFromParent();
        }
        deleteInstructionQueue.clear();
      }
    }

    return PreservedAnalyses::all();
  };
};
}  // namespace

// This is just boilerplate to register the pass.
// Do do not need to modify this code.
extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "CocoInstCombine", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "coco-instcombine") {
                    FPM.addPass(CocoInstCombine());
                    return true;
                  }
                  return false;
                });
          }};
}

#include <iostream>

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

struct CocoInstCombine : PassInfoMixin<CocoInstCombine> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
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
            if (precomputeConstantValues(I)) {
              auto NextI = std::next(I);
              I->removeFromParent();
              I = NextI;
            }
            break;
          }
        }
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

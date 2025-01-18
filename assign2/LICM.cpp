#include <llvm/IR/IRBuilder.h>

#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
bool isInvariant(Value* val, Loop* loop, DenseSet<Value *>& invariants) {
  // We can not move the location of the instruction if it has side effects.
  // Cache, return directly.
  if (invariants.count(val))
    return true;

  if (isa<Argument>(val)) {
    return true;
  }

  if (isa<Constant>(val)) {
    return true;
  }

  if (!isSafeToSpeculativelyExecute(val)) {
    return false;
  }

  // We only consider instructions (Register to register).
  if (auto I = dyn_cast<Instruction>(val)) {
    // The dependent operands are outside the loop.
    if (!loop->contains(I)) {
      invariants.insert(val);
      return true;
    }

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
        const auto lhs = I->getOperand(0);
        const auto rhs = I->getOperand(1);

        if (isInvariant(lhs, loop, invariants)) {
          // outs() << "lhs " << *I << "\n";
          invariants.insert(lhs);
        }

        if (isInvariant(rhs, loop, invariants)) {
          invariants.insert(rhs);
        }

        // Chain of dependencies.
        if (invariants.count(lhs) && invariants.count(rhs)) {
          // outs() << "instruction " << *I << "\n";
          invariants.insert(val);
          return true;
        }
        break;
      }
      default:
        return false;
    }
  }

  return false;
}

struct CocoLICM : PassInfoMixin<CocoLICM> {
  Loop *curLoop = nullptr;
  DominatorTree *DT = nullptr;
  LoopInfo *LI = nullptr;
  PreservedAnalyses run(LoopNest &L, LoopAnalysisManager &AM,
                        LoopStandardAnalysisResults &AR, LPMUpdater &U) {
    LI = &AR.LI;
    DT = &AR.DT;
    curLoop = &L.getOutermostLoop();
    SmallVector<Instruction*> removeQueue;

    BasicBlock* currLoopHeader = curLoop->getHeader();
    BasicBlock* preHeader = curLoop->getLoopPreheader();
    Instruction *terminator = preHeader->getTerminator();
    IRBuilder<> Builder(terminator);

    // Get all the nodes that are dominated by the current headers.
    SmallVector<BasicBlock *> Descendants;
    DenseSet<Value *> invariants;
    DT->getDescendants(currLoopHeader, Descendants);

    // Try to get all the basic blocks that are within the current loop.
    for (auto b : Descendants) {
      if (curLoop->contains(b)) {
        // Not sure if it's in a sub-loop.
        for (auto &inst : b->getInstList()) {
          if (isInvariant(&inst, curLoop, invariants)) {
            Instruction *newInstruction = inst.clone();
            Builder.SetInsertPoint(preHeader, preHeader->getTerminator()->getIterator());
            Builder.Insert(newInstruction);

            removeQueue.push_back(&inst);
            inst.replaceAllUsesWith(newInstruction);
            errs() << "remove instruction " << inst << "\n";
          };
        }
      }
    }

    // for (auto val : invariants) {
    //   if (auto inst = dyn_cast<Instruction>(val)) {
    //     errs() << "remove instruction " << *inst << "\n";
    //   }
    // }

    for (auto I : removeQueue) {
      // Check if the instruction is already removed.
      if (!I) { continue; }

      if (!I->getParent()) { continue; }

      if (I->isSafeToRemove()) {
        I->eraseFromParent();
      }
    }

    return PreservedAnalyses::none();
  }
};
}  // namespace

// This is just boilerplate to register the pass.
// Do do not need to modify this code.
extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "coco-licm", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, LoopPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "coco-licm") {
                    FPM.addPass(CocoLICM());
                    return true;
                  }
                  return false;
                });
          }};
}

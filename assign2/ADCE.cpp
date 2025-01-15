#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
bool isTriviallyAlive(Instruction* inst) {
  if (inst->mayHaveSideEffects()) {
    return true;
  }
  if (inst->isTerminator()) {
    return true;
  }
  if (inst->isVolatile()) {
    return true;
  }
  if (inst->mayWriteToMemory()) {
    return true;
  }
  if (isa<CallInst>(inst)) {
    // Check if this instruction is a function call.
    return true;
  }

  return false;
}
struct CocoADCE : PassInfoMixin<CocoADCE> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {\
    // Dummy code for depth first visiting of blocks.
    SmallVector<Instruction *> work_list = {};
    df_iterator_default_set<BasicBlock *> Reachable;


    for (BasicBlock *BB : depth_first_ext(&F, Reachable)) {
      for (auto it = BB->getInstList().begin(); it != BB->getInstList().end();) {
        // Prevent delete the iterator container.
        auto &inst = *it;

        if (isTriviallyAlive(&inst)) {
          work_list.push_back(&inst);
          ++it;
        } else if (inst.use_empty()) {
          // Trivially dead code.
          it = inst.eraseFromParent();
        } else {
          ++it;
        }
      }
    }

    return PreservedAnalyses::all();
  }
};
}  // namespace

// This is just boilerplate to register the pass.
// Do do not need to modify this code.
extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "CocoADCE", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "coco-adce") {
                    FPM.addPass(CocoADCE());
                    return true;
                  }
                  return false;
                });
          }};
}

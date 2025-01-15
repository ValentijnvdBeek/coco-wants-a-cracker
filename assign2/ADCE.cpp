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
  // if (inst->mayWriteToMemory()) {
  //   return true;
  // }
  if (isa<CallInst>(inst)) {
    // Check if this instruction is a function call.
    return true;
  }

  return false;
}

struct CocoADCE : PassInfoMixin<CocoADCE> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {\
    // Dummy code for depth first visiting of blocks.
    SmallVector<Instruction *> work_list;
    DenseSet<Instruction *> live_list;
    df_iterator_default_set<BasicBlock *> Reachable;

    errs() << F.getName() << "\n";

    for (BasicBlock *BB : depth_first_ext(&F, Reachable)) {
      for (auto &inst : BB->getInstList()) {
        errs() << inst << "\n";
        if (isTriviallyAlive(&inst)) {
          work_list.push_back(&inst);
          live_list.insert(&inst);
        }
        if (inst.use_empty() && !isTriviallyAlive(&inst)) {
          errs() << "[ADCE] mark instruction dead: " << inst << "\n";
        }
      }
    }

    while (!work_list.empty()) {
      auto inst = work_list.pop_back_val();
      if (Reachable.count(inst->getParent())) {
        for (auto &op : inst->operands()) {
          if (auto local_inst = dyn_cast<Instruction>(op)) {
            // Check if op is an instruction in the list.
            // Add everything to the live_list.
            if (!live_list.count(local_inst)) {
              live_list.insert(local_inst);
              // Push the new instruction into the work list.
              work_list.push_back(local_inst);
            }
          }
        }
      }
    }

    for (auto B : Reachable) {
      for (auto &inst : B->getInstList()) {
        if (live_list.find(&inst) == live_list.end()) {
          // Drop to avoid def-use cycle.
          inst.dropAllReferences();
        }
      }
    }

    for (auto B : Reachable) {
      for (auto it = B->getInstList().begin(); it != B->getInstList().end();) {
        auto &inst = *it++;

        if (live_list.find(&inst) == live_list.end()) {
          // It's not alive.
          inst.eraseFromParent();

          errs() << "[ADCE] delete instruction: " << inst.getName() << "\n";
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

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
struct CocoSanitizer : PassInfoMixin<CocoSanitizer> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    for (auto &f : M.functions()) {
      auto BlockPointer = f.begin();
      uint32_t BlockSize = f.size();
      for (uint32_t Index = 0; Index < BlockSize; Index++) {
        BasicBlock &Block = *BlockPointer;
        for (Instruction &Instr : Block) {
          CallInst *Call = dyn_cast<CallInst>(&Instr);
          if (Call == nullptr) continue;

          // Get the function of the call, skip if indirect
          Function *Fun = Call->getCalledFunction();
          if (Fun == nullptr) continue;

          StringRef name = Fun->getName();
          if (name.compare(StringRef("free")) == 0) {
            errs() << "Replacing " << name << "\n";
            Fun->setName("_coco_free");
          } else if (name.compare(StringRef("malloc")) == 0) {
            errs() << "Replacing " << name << "\n";
            Fun->setName("_coco_malloc");
          } else {
            errs() << Instr.getOpcodeName() << " " << name << "\n";
          }
        }
        BlockPointer++;
      }
    }
    return PreservedAnalyses::all();
  }
};
}  // namespace

// This is just boilerplate to register the pass.
// You do not need to modify this code.
extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "CocoSanitizer", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &PM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "coco-sanitizer") {
                    PM.addPass(CocoSanitizer());
                    return true;
                  }
                  return false;
                });
          }};
}

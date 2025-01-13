#include <iostream>
#include <unordered_map>

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

using namespace llvm;

namespace {
struct CocoInlinerPass : PassInfoMixin<CocoInlinerPass> {
  bool tryInline(Function &f, CallInst &call) {
    ;
    InlineFunctionInfo info;
    auto InlineRes = InlineFunction(call, info);
    assert(InlineRes.isSuccess() && "Expected inlining to succeed");
    return true;
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    CallGraph cg(M);
    std::unordered_map<const char *, unsigned long> FunctionSizes;

    // Get the total instruction count of the function
    for (auto &f : M.functions()) {
      long unsigned InstructCount = 0;
      long unsigned BlockSize = f.size();
      auto BlockPointer = f.begin();
      for (long unsigned Index = 0; Index < BlockSize; Index++) {
        InstructCount += BlockPointer->size();
        fprintf(stderr, "[Inliner] block %ld: %ld instructions\n", Index,
                BlockPointer->size());
        BlockPointer++;
      }

      fprintf(stderr, "Function %s: blocks %ld, instructions %ld\n",
              f.getName().data(), BlockSize, InstructCount);
      FunctionSizes[f.getName().data()] = InstructCount;
    }

    // Loop through the call graph.
    for (auto &funcAndNode : cg) {
      // Function that we originate from
      Function *f = const_cast<Function *>(funcAndNode.first);

      // I don't actually know what this is actually
      std::unique_ptr<CallGraphNode> &CGN = funcAndNode.second;

      // Ensure that neither both are mapped to value. These are
      // dynamically casted, so they can be null.
      if (!f || !CGN->getFunction()) continue;

      // Store all the calls in the function in a list so we can
      // delete at the end. Doing it earlier causes a panic.
      std::vector<CallInst *> calls;
      for (auto &Instr : instructions(f)) {
        auto *ci = dyn_cast<CallInst>(&Instr);
        if (!ci) continue;

        const Function *CalledFunction = ci->getCalledFunction();
        if (!CalledFunction) continue;

        const char *FuncName = CalledFunction->getName().data();
        if (CalledFunction->isDeclaration()) {
          fprintf(stderr, "[Inliner] %s is declared outside of the unit\n",
                  FuncName);
          continue;
        }

        // For this heuristic 5 is the magic number, anything more
        // causes big regressions. Maybe try seeing if number of
        // variables is more effective? Definitely a thing to look \
        // into.
        if (FunctionSizes[FuncName] > 5) continue;

        fprintf(stderr, "[Inliner] inline %s\n", FuncName);
        calls.push_back(ci);
      }

      for (auto &ci : calls) {
        tryInline(*f, *ci);
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
  return {LLVM_PLUGIN_API_VERSION, "CocoInliner", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &PM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "coco-inliner") {
                    PM.addPass(CocoInlinerPass());
                    return true;
                  }
                  return false;
                });
          }};
}

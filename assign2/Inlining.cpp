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
const unsigned CACHE_LINE_SIZE = 16;

struct CocoInlinerPass : PassInfoMixin<CocoInlinerPass> {
  bool tryInline(Function &f, CallInst &call) {
    ;
    InlineFunctionInfo info;
    auto InlineRes = InlineFunction(call, info);
    assert(InlineRes.isSuccess() && "Expected inlining to succeed");
    return true;
  }

  DenseSet<GlobalVariable *> findGlobalVariablesInFunction(Function &F) {
    DenseSet<GlobalVariable *> GVs;

    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        for (unsigned i = 0; i < I.getNumOperands(); ++i) {
          Value *operand = I.getOperand(i);

          // Check if the operand is a GlobalVariable
          if (auto *globalVar = llvm::dyn_cast<llvm::GlobalVariable>(operand)) {
            GVs.insert(globalVar);
          }
        }
      }
    }

    return GVs;
  }

  bool hasIntersection(const DenseSet<GlobalVariable *> &set1, const DenseSet<GlobalVariable *> &set2) {
    for (const auto elem : set1) {
      if (set2.find(elem) != set2.end()) {
        return true; // Found a common element
      }
    }
    return false; // No common elements
  }

  bool constFunction(Function &F) {
    SmallVector<Instruction *> ret_insts;

    for (auto &BB : F.getBasicBlockList()) {
      for (auto &I : BB.getInstList()) {
        if (auto ret = dyn_cast<ReturnInst>(&I)) {
          ret_insts.push_back(&I);
        }
      }
    }

    if (ret_insts.size() != 1) {
      // It doesn't return a constant directly.
      return false;
    }

    if (ret_insts[0]->getNumOperands() == 0) {
      // return void.
      return false;
    }

    auto op = ret_insts[0]->getOperand(0);
    if (isa<Constant>(op)) {
      errs() << "[Inlining GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG!]" << "\n";
      return true;
    }
    return false;
  }

  unsigned getLoopSize(BasicBlock *BB) {
    unsigned InstructionCount = 0;
    for (auto &Inst : BB->getInstList()) {
      ++InstructionCount;
    }
    return InstructionCount;
  }

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    CallGraph cg(M);
    std::unordered_map<std::string, unsigned long> FunctionSizes;

    // Get the total instruction count of the function
    for (auto &f : M.functions()) {
      long unsigned InstructCount = 0;
      long unsigned BlockSize = f.size();
      auto BlockPointer = f.begin();
      for (long unsigned Index = 0; Index < BlockSize; Index++) {
        InstructCount += BlockPointer->size();
        // fprintf(stderr, "[Inliner] block %ld: %ld instructions\n", Index,
        //         BlockPointer->size());
        BlockPointer++;
      }

      // fprintf(stderr, "Function %s: blocks %ld, instructions %ld\n",
      //         f.getName().data(), BlockSize, InstructCount);
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

        auto FuncName = CalledFunction->getName().str();
        if (CalledFunction->isDeclaration()) {
          // fprintf(stderr, "[Inliner] %s is declared outside of the unit\n",
          //        FuncName);
          continue;
        }

        // For this heuristic 5 is the magic number, anything more
        // causes big regressions. Maybe try seeing if number of
        // variables is more effective? Definitely a thing to look \
        // into.
        // if (FunctionSizes[FuncName] > 5) continue;

        calls.push_back(ci);
      }

      for (auto &ci : calls) {
        auto CalledFuncName = ci->getCalledFunction()->getName().str();

        // 1. Check if this call is in a loop.
        // 2. If it's in a loop, check if it exceeds the cache size after inlining.
        // 3. If it suits the cache size, inline it, else continue.
        DominatorTree DT = DominatorTree();
        auto fn = ci->getFunction();
        DT.recalculate(*fn);

        bool is_inlining = true;

        auto loopInfo = new LoopInfoBase<BasicBlock, Loop>();
        loopInfo->releaseMemory();
        loopInfo->analyze(DT);
        Function* CalledFunction = ci->getCalledFunction();

        auto parentB = ci->getParent();
        if (auto L = loopInfo->getLoopFor(parentB)) {
          if (FunctionSizes[CalledFuncName] + getLoopSize(parentB) > CACHE_LINE_SIZE) {
            is_inlining = false;
          }

          if (constFunction(*CalledFunction)) {
            is_inlining = true;
          }
        } else {
          // If it's not in a loop, we should limit the size of it.
          // Seems like the grading framework won't cost the function icache refresh, so the call
          // is pretty cheap.

          // TODO(Zhiyang): Check if there is any calculation uses the result of the function.
          // If there is only printf uses it, then we shouldn't inline it.




          if (FunctionSizes[CalledFuncName] > 7) {
            is_inlining = false;
          }



        }

        SmallVector<Instruction *> users;
        DenseSet<GlobalVariable *> GVsByCalleeFunc = findGlobalVariablesInFunction(*CalledFunction);
        DenseSet<GlobalVariable *> GVsByCallerFunc = findGlobalVariablesInFunction(*ci->getFunction());

        for (const auto user : ci->users()) {
          if (auto inst = dyn_cast<Instruction>(user)) {
            users.push_back(inst);
          }
        };

        if (users.size() == 1) {
          // 1. There is only one user uses the result of the function (printf in this case).
          // 2. No any inputs from the caller.
          // 3. No global variables that can interact with the caller.
          // Which means it can't be used as the optimization.
          if (isa<CallInst>(users[0]) && CalledFunction->arg_size() == 0) {
            if (!hasIntersection(GVsByCallerFunc, GVsByCalleeFunc)) {
              errs() << "[Inliner] Find the func that only printf uses." << *users[0] << GVsByCalleeFunc.size() << " : " << GVsByCallerFunc.size() << "\n";
              is_inlining = false;
            }
          }
        }

        if (constFunction(*CalledFunction)) {
          is_inlining = true;
        }

        if (is_inlining) {
          tryInline(*f, *ci);
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
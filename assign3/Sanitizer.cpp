#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {

void replaceFreeAndMalloc(Instruction &Instr) {
  CallInst *Call = dyn_cast<CallInst>(&Instr);
  if (Call == nullptr) return;

  // Get the function of the call, skip if indirect
  Function *Fun = Call->getCalledFunction();
  if (Fun == nullptr) return;

  StringRef name = Fun->getName();
  if (name.compare(StringRef("free")) == 0) {
    //            errs() << "Replacing " << name << "\n";
    Fun->setName("_coco_free");
  } else if (name.compare(StringRef("malloc")) == 0) {
    //            errs() << "Replacing " << name << "\n";
    Fun->setName("_coco_malloc");
  } else {
    //            errs() << Instr.getOpcodeName() << " " << name << "\n";
  }
}

void calculatePointerAddress(Module &M, IRBuilder<> &Builder,
                             Function *LoadStoreCheck, Instruction *Instr) {
  const DataLayout &DataLayout = M.getDataLayout();
  StoreInst *StoreInstr = dyn_cast<StoreInst>(Instr);
  if (StoreInstr == nullptr) {
    return;
  }

  Type *UInt64Ty = Type::getInt64Ty(M.getContext());
  Type *VoidTy = Type::getVoidTy(M.getContext());

  // Cast the array pointer to an uint64 number. This assumes a 64-bit
  // host computer. This is needed since LLVM IR doesn't decay arrays
  // into pointers, so we need to cast it manually.
  Value *CastedPtr =
      Builder.CreatePointerCast(StoreInstr->getOperand(1), UInt64Ty);
  Builder.CreateCall(LoadStoreCheck, {CastedPtr});
  return;
}

struct CocoSanitizer : PassInfoMixin<CocoSanitizer> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    Type *UInt64Ty = Type::getInt64Ty(M.getContext());
    Type *VoidTy = Type::getVoidTy(M.getContext());
    // Get or create the function _coco_check_load_store_ptr
    FunctionType *funcType = FunctionType::get(VoidTy, {UInt64Ty}, false);
    Function *LoadStoreCheck =
        llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                               "_coco_check_load_store_ptr", &M);

    if (LoadStoreCheck == nullptr) {
      errs() << "!!Cannot find coco checking function!!\n";
    }

    for (auto &f : M.functions()) {
      for (BasicBlock &Block : f) {
        for (Instruction &Instr : Block) {
          replaceFreeAndMalloc(Instr);
          IRBuilder<> Builder(&Instr);
          calculatePointerAddress(M, Builder, LoadStoreCheck, &Instr);
        }
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

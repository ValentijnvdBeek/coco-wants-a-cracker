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
  GetElementPtrInst *GEPInstr = dyn_cast<GetElementPtrInst>(Instr);
  if (GEPInstr == nullptr) {
    return;
  }

  // Get the size of the resulting pointer type in bytes, we need this
  // to calculate the offset of our pointer
  const uint32_t AllocSize =
      DataLayout.getTypeAllocSize(GEPInstr->getResultElementType())
          .getFixedValue();
  uint32_t AccumulatedValue = 0;

  // Add all the various values in getelementptr together, I don't know
  // exactly what this does. This is just a dirty hack for now to get
  // some results
  for (uint32_t Index = 0; Index < GEPInstr->getNumOperands(); Index++) {
    const Value *Value = GEPInstr->getOperand(Index);
    const ConstantInt *Offset = dyn_cast<ConstantInt>(Value);
    if (Offset == nullptr) continue;

    const uint32_t OffsetValue = Offset->getSExtValue();
    errs() << Offset->getSExtValue() << " * " << AllocSize << ", ";
    AccumulatedValue += OffsetValue + AllocSize;
  }

  errs() << "Acc Value" << AccumulatedValue << "\n";
  Type *UInt64Ty = Type::getInt64Ty(M.getContext());
  Type *VoidTy = Type::getVoidTy(M.getContext());

  // Cast the array pointer to an uint64 number. This assumes a 64-bit
  // host computer. This is needed since LLVM IR doesn't decay arrays
  // into pointers, so we need to cast it manually.
  Value *CastedPtr =
      Builder.CreatePointerCast(GEPInstr->getOperand(0), UInt64Ty);
  Value *args[] = {CastedPtr,
                   ConstantInt::get(UInt64Ty, APInt(64, AccumulatedValue))};
  Builder.CreateCall(LoadStoreCheck, args);
  return;
}

struct CocoSanitizer : PassInfoMixin<CocoSanitizer> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &) {
    Type *UInt64Ty = Type::getInt64Ty(M.getContext());
    Type *VoidTy = Type::getVoidTy(M.getContext());
    // Get or create the function _coco_check_load_store_ptr
    FunctionType *funcType =
        FunctionType::get(VoidTy, {UInt64Ty, UInt64Ty}, false);
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

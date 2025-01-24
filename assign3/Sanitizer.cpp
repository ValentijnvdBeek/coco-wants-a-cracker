#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
constexpr uint8_t _coco_buffer_size = 32;

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

void replaceToReleaseStackMemory(Module &M, IRBuilder<> &Builder, Instruction &Instr) {
  CallInst *Call = dyn_cast<CallInst>(&Instr);
  if (Call == nullptr) return;

  // Get the function name of the call.
  Function *Fun = Call->getCalledFunction();

  if (Fun == nullptr) return;

  // Filter out the built-in function.
  if (Fun->isDeclaration() || Fun->isIntrinsic()) return;
  auto CalleeFuncName = Call->getCalledFunction()->getName();

  // Get the relase stack funuction.
  LLVMContext &ctx = Fun->getContext();
  FunctionCallee releaseFunc = M.getOrInsertFunction(
    "releaseAllocatedMemory", Type::getVoidTy(ctx), Type::getInt8PtrTy(ctx)
  );

  IRBuilder<> AfterBuilder(Call->getNextNode());
  // Insert the release function after calling the function.
  Constant *ReleasedFunctionName = Builder.CreateGlobalStringPtr(CalleeFuncName);
  AfterBuilder.CreateCall(releaseFunc, {ReleasedFunctionName});
}

void replaceAllocInstWithSafeAlloc(Module &M, IRBuilder<> &Builder, Instruction *Inst, SmallVector<Instruction *>& delQueue) {
  // The general algorithm for this replacement is:
  // 1. find all the alloca and create a larger alloca to hold the red zone.
  // 2. call the red zone initialization function to initialize the larger allocated space,
  // the new alloc pointer as the arg.
  // 3. replace all the uses of old alloca (not the new alloca one) with the return function pointer.
  // (If you replace the old Alloca with the new Alloca, the size can't be matched)
  // 4. delete all the old Allocas.
  AllocaInst *Alloca = dyn_cast<AllocaInst>(Inst);
  if (Alloca == nullptr) return;

  if (Alloca->getFunction()->isDeclaration() || Alloca->getFunction()->isIntrinsic()) return;

  auto array_size = Alloca->getArraySize();
  auto numType = dyn_cast<ConstantInt>(array_size);
  if (numType == nullptr) return;

  auto AllocatedTy = Alloca->getAllocatedType();
  Type *UInt64Ty = Type::getInt64Ty(M.getContext());
  const DataLayout &DL = M.getDataLayout();

  AllocaInst* newAlloca;
  uint64_t requestedSize = 0;

  if (AllocatedTy->getTypeID() == Type::ArrayTyID) {
    // Get the size of the Type.
    const uint64_t arraySizeInBytes = DL.getTypeAllocSize(AllocatedTy);
    const uint64_t elementSizeInBytes = DL.getTypeAllocSize(AllocatedTy->getArrayElementType());

    // The coco_buffer_size is 32 bytes that are big enough for all the types involved.
    // Like (8 * n + 32) / 8 and (32 * n + 32) / 32 are both dividable.
    requestedSize = arraySizeInBytes * numType->getZExtValue();
    const uint64_t allocaSize = arraySizeInBytes * numType->getZExtValue() + _coco_buffer_size * 2;
    const uint64_t newElementsNum = allocaSize / elementSizeInBytes;

    const auto newArray = ArrayType::get(Alloca->getAllocatedType()->getArrayElementType(), newElementsNum);
    // errs() << "new array" << *newArray << "\n";

    newAlloca = Builder.CreateAlloca(
      newArray
    );

    // We need to replace all the GEPs since the array type is changed.
    // for (auto user : Alloca->users()) {
    //   if (auto GEP = dyn_cast<GetElementPtrInst>(user)) {
    //     GEP->setSourceElementType(newArray);
    //   }
    // }
  } else {
    const uint64_t typeSize = DL.getTypeAllocSize(AllocatedTy);
    requestedSize = typeSize * numType->getZExtValue();
    const uint64_t totalNewTypeSize = typeSize * numType->getZExtValue() + _coco_buffer_size * 2;
    const uint64_t totalNewNum = totalNewTypeSize / typeSize;
    // errs() << "new type" << *AllocatedTy << "\n";
    // errs() << "new num " << totalNewNum << "\n";
    const auto totalNumVal = ConstantInt::get(UInt64Ty, totalNewNum);

    newAlloca = Builder.CreateAlloca(
      AllocatedTy,
      totalNumVal
    );
  }

  auto newSizeVal = ConstantInt::get(UInt64Ty, requestedSize);
  auto fname = Alloca->getFunction()->getName();
  Constant *FNameIR = Builder.CreateGlobalStringPtr(fname);

  // Try to match the function signature.
  // There is no native void * in llvm, so use i8*.
  auto FuncSignature = FunctionType::get(
     Type::getInt8PtrTy(M.getContext()),
     {
     Type::getInt8PtrTy(M.getContext()),
     UInt64Ty,
     UInt64Ty
    },
    false
  );

  FunctionCallee initializeStackMemorySpace = M.getOrInsertFunction(
    "_coco_stack_alloc", FuncSignature
  );

  // Create the call to the stack alloc function.
  // Alloca->replaceAllUsesWith(newAlloca);
  delQueue.push_back(Alloca);

  // Cast the pointer to uint, otherwise we need to bitcast.
  Value* PtrToAddr = Builder.CreatePtrToInt(newAlloca, UInt64Ty);

  auto Call = Builder.CreateCall(initializeStackMemorySpace, {FNameIR, PtrToAddr, newSizeVal});
  // Match the returned type of call with the new alloca instruction.
  // The new alloca is larger, so we should keep use the old one.
  // This is a little bit tricky, but it works.
  Value *ReturnedPtr = Builder.CreateBitCast(Call, Alloca->getType());
  Alloca->replaceAllUsesWith(ReturnedPtr);
}

void calculatePointerAddress(Module &M, IRBuilder<> &Builder,
                             Function *LoadStoreCheck, Instruction *Instr) {
  const DataLayout &DataLayout = M.getDataLayout();
  StoreInst *StoreInstr = dyn_cast<StoreInst>(Instr);
  if (StoreInstr == nullptr) {
    return;
  }

  // Check the source of the store pointer, if it is on the stack we can ignore
  // it. This is only because of the assignment text, in real programs, this is
  // stil a bad idea.
  Value *PointerVal = StoreInstr->getOperand(1);
  if (dyn_cast<AllocaInst>(PointerVal) != NULL) {
    return;
  }

  // Cast the array pointer to an uint64 number. This assumes a 64-bit
  // host computer. This is needed since LLVM IR doesn't decay arrays
  // into pointers, so we need to cast it manually.
  Type *UInt64Ty = Type::getInt64Ty(M.getContext());
  Type *VoidTy = Type::getVoidTy(M.getContext());
  Value *CastedPtr = Builder.CreatePointerCast(PointerVal, UInt64Ty);
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
    SmallVector<Instruction *> deletionQueue;

    if (LoadStoreCheck == nullptr) {
      errs() << "!!Cannot find coco checking function!!\n";
    }

    for (auto &f : M.functions()) {
      for (BasicBlock &Block : f) {
        // The replace function will insert a new instruction, and it will make
        // the loop runs infinitely, so we need to collect the list of instructions
        // first and then traverse it.
        SmallVector<Instruction*> originalInsts;
        for (auto &Instr : Block) {
          originalInsts.push_back(&Instr);
        }

        for (Instruction *Instr : originalInsts) {
          replaceFreeAndMalloc(*Instr);
          IRBuilder<> Builder(Instr);
          calculatePointerAddress(M, Builder, LoadStoreCheck, Instr);
          replaceToReleaseStackMemory(M, Builder, *Instr);
          replaceAllocInstWithSafeAlloc(M, Builder, Instr, deletionQueue);
        }
      }
    }

    for (auto inst : deletionQueue) {
      inst->eraseFromParent();
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

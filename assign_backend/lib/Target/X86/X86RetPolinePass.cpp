#include "MCTargetDesc/X86MCTargetDesc.h"
#include "X86.h"
#include "X86FrameLowering.h"
#include "X86InstrInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDwarf.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

#define X86_RETPOLINE_PASS_NAME "Add retpolines to X86 pass"

namespace {

class X86RetPoline : public MachineFunctionPass {

public:
  static char ID;

  X86RetPoline() : MachineFunctionPass(ID) {

    initializeX86RetPolinePass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return X86_RETPOLINE_PASS_NAME; }
};

char X86RetPoline::ID = 0;

bool X86RetPoline::runOnMachineFunction(MachineFunction &MF) {
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  MCInst MCI;
  DebugLoc DL;

  if (!TII)
    return false;

  std::vector<MachineBasicBlock *> Blocks;
  auto MBS = MF.begin();
  auto MBE = MF.end();
  bool Modified = false;
  for (auto &MBI = MBS; MBI != MBE; MBI++) {
    auto &MBB = *MBI;

    // Ignore blocks that we just added to begin with
    bool Found = false;

    for (auto *MP : Blocks) {
      if (MP == &*MBI)
        Found = true;
    }
    if (Found)
      continue;

    const BasicBlock *BB = MBB.getBasicBlock();
    if (BB == nullptr)
      continue;

    auto *RetPtr = dyn_cast<ReturnInst>(BB->getTerminator());
    if (RetPtr != nullptr) {
      Modified = true;

      // Create a guard block that traps the speculative execution
      MachineBasicBlock *LoopGuard =
          MF.CreateMachineBasicBlock(MBB.getBasicBlock());
      LoopGuard->setHasAddressTaken(); // Target of a indirect branch sometimes
      LoopGuard->addSuccessor(LoopGuard); // Recursive call
      MF.push_back(LoopGuard);

      // Retpoline implementation. The pause and jump cause the indirect branch
      // to idle on this function call. We also add the LFENCE instruction
      // in-line with implementation in RetpolineThunkInserter::populateThunk,
      // which ensures that it also works for AMD processors.
      BuildMI(*LoopGuard, LoopGuard->end(), DL, TII->get(X86::PAUSE));
      BuildMI(*LoopGuard, LoopGuard->end(), DL, TII->get(X86::LFENCE));
      BuildMI(*LoopGuard, LoopGuard->end(), DL, TII->get(X86::JMP_1))
          .addMBB(LoopGuard);

      // Create a call that returns to the original function
      MCSymbol *TargetSym =
          MF.getContext().createTempSymbol(); // Generate a name
      MachineBasicBlock *ReturnNew =
          MF.CreateMachineBasicBlock(MBB.getBasicBlock());
      ReturnNew->setHasAddressTaken(); // Target of an indirect branch
      ReturnNew->addLiveIn(X86::RSP);  // Indicate that we use stack pointer

      MF.push_back(ReturnNew);

      // Increment the stack pointer so that the old return location is removed
      BuildMI(*ReturnNew, ReturnNew->end(), DL, TII->get(X86::ADD64ri8),
              X86::RSP)
          .addReg(X86::RSP)
          .addImm(8);
      BuildMI(*ReturnNew, ReturnNew->end(), DL, TII->get(X86::RETQ));

      // Delete the terminator that we have found so that we can replace it
      auto I = MBB.getFirstTerminator();
      I->eraseFromParent();

      // Set the temporary target name to the very end of the LoopGuard section.
      // In my testing, this put it always on the right place, but using
      // ReturnNew->front Does make more theoretical sense.
      LoopGuard->back().setPostInstrSymbol(MF, TargetSym);
      BuildMI(&MBB, DL, TII->get(X86::CALL64pcrel32)).addSym(TargetSym);

      // Register the pointers to our data structure so we don't reiterate over
      // them. MF.push_back modifies the vector we are currently modifying so,
      // we need to take extra care
      Blocks.push_back(ReturnNew);
      Blocks.push_back(LoopGuard);

      // Tell LLVM that we have added two additional sections to our program
      MBB.addSuccessor(LoopGuard);
      MBB.addSuccessor(ReturnNew);
    }
  }

  return Modified;
}

} // end of anonymous namespace

INITIALIZE_PASS(X86RetPoline, "x86-retpoline-pass",

                X86_RETPOLINE_PASS_NAME,

                false, // is CFG only?

                false // is analysis?

)

namespace llvm {

FunctionPass *createX86RetPolinePass() { return new X86RetPoline(); }

} // namespace llvm
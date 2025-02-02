//===-- MachineFunctionPrinterPass.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MachineFunctionPrinterPass implementation.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/RandomNumberGenerator.h"
#include "llvm/Support/raw_ostream.h"

#include <random>

using namespace llvm;

namespace {
/// NOPInserterPass - This is a pass to dump the IR of a
/// MachineFunction.
///
///
static cl::opt<unsigned>
  NUM_NOOP("num_noop", cl::init(1), cl::Hidden);

struct NOPInserter : public MachineFunctionPass {
  static char ID;

  raw_ostream &OS;
  const std::string Banner;

  // Get parameter from cl command line.

  NOPInserter() : MachineFunctionPass(ID), OS(dbgs()) { }
  NOPInserter(raw_ostream &os, const std::string &banner)
      : MachineFunctionPass(ID), OS(os), Banner(banner) {}

  StringRef getPassName() const override { return "Add NOPs to Machine Function"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addUsedIfAvailable<SlotIndexes>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override {
    const TargetInstrInfo* TII = MF.getSubtarget().getInstrInfo();
    MCInst MCI;

    if (!TII) {
      return false;
    }

    // Randomize the number of nops
    auto RNG =
      MF.getFunction().getParent()->createRNG(this);

    bool changed = false;

    DebugLoc DL;
    /* Follow this to create a NOOP.
      void AArch64InstrInfo::insertNoop(MachineBasicBlock &MBB,
        MachineBasicBlock::iterator MI) const {
      DebugLoc DL;
      BuildMI(MBB, MI, DL, get(AArch64::HINT)).addImm(0);
      }

      MCInst AArch64InstrInfo::getNop() const {
        return MCInstBuilder(AArch64::HINT).addImm(0);
      }
     */
    // Nop locations.
    // Before the first/last instruction in a function
    // Before each call site
    // After each call site

    // void MipsInstrInfo::
    // insertNoop(MachineBasicBlock & MBB, MachineBasicBlock::iterator MI) const {
    //   DebugLoc DL;
    //   BuildMI(MBB, MI, DL, get(Mips::NOP));
    // }
    for (auto &MBB : MF) {
      if (MBB.empty()) {
        // Ignore the emptiness case.
        continue;
      }

      TII->getNoop(MCI);

      // Run random number times.
      for (uint64_t i = 0; i < (*RNG)() % NUM_NOOP; ++i) {
        // TargetInstrInfo tells us how to insert the noop.
        // https://llvm.org/doxygen/classllvm_1_1TargetInstrInfo.html
        // Skip unreachable blocks (they have getNumber() == -1)
        if (MBB.getNumber() < 0)
          continue;

        // Add nop before the first instruction in a function.
        BuildMI(MBB, MBB.begin(), DL, TII->get(MCI.getOpcode()));

        // Add nop before the last instruction in a function.
        BuildMI(MBB, MBB.end(), DL, TII->get(MCI.getOpcode()));

        // before and after the call site.
        for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {

          if (MI->isCall()) {
            BuildMI(MBB, MI, DL, TII->get(MCI.getOpcode()));

            // Get the next instruction after the call.
            auto Next = std::next(MI);

            if (Next != MBB.end()) {
              BuildMI(MBB, Next, DL, TII->get(MCI.getOpcode()));
            }
          }
        }

        changed = true;
      }
    }

    return changed;
  }
};

char NOPInserter::ID = 0;
}

char &llvm::NOPInserterPassID = NOPInserter::ID;
INITIALIZE_PASS(NOPInserter, "machineinstr-add-nops",
                "Machine Function Printer", false, false)

namespace llvm {
/// Returns a newly-created MachineFunction Printer pass. The
/// default banner is empty.
///
MachineFunctionPass *createNOPInserterPass(raw_ostream &OS,
                                                      const std::string &Banner){
  return new NOPInserter(OS, Banner);
}

}

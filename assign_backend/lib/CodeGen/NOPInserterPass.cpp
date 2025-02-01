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
struct NOPInserter : public MachineFunctionPass {
  static char ID;

  raw_ostream &OS;
  const std::string Banner;

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
    if (!TII) {
      return false;
    }

    // Randomize the number of nops
    auto RNG =
      MF.getFunction().getParent()->createRNG(this);

    bool changed = false;

    // Nop locations.
    // Before the first/last instruction in a function
    // Before each call site
    // After each call site
    for (auto &MBB : MF) {
      if (MBB.empty()) {
        // Ignore the emptiness case.
        continue;
      }
      // TargetInstrInfo tells us how to insert the noop.
      // https://llvm.org/doxygen/classllvm_1_1TargetInstrInfo.html

      // Add nop before the first instruction in a function.
      TII->insertNoop(MBB, MBB.begin());

      // Add nop before the last instruction in a function.
      TII->insertNoop(MBB, MBB.end());

      // before and after the call site.
      for (auto MI = MBB.begin(); MI != MBB.end(); ++MI) {
        if (MI->isCall()) {
          TII->insertNoop(MBB, MI);
          // Get the next instruction after the call.
          auto next = MI++;

          TII->insertNoop(MBB, next);
        }
      }

      changed = true;
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

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
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

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
    if (!llvm::isFunctionInPrintList(MF.getName()))
      return false;
    OS << "# " << Banner << ":\n";
    MF.print(OS, getAnalysisIfAvailable<SlotIndexes>());
    return false;
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

#include "X86.h"
#include "X86InstrInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Pass.h"

using namespace llvm;

#define X86_MACHINEINSTR_PRINTER_PASS_NAME "Dummy X86 machineinstr printer pass"

namespace {

class X86MachineInstrPrinter : public MachineFunctionPass {
public:
    static char ID;

    explicit X86MachineInstrPrinter() : MachineFunctionPass(ID) {
        initializeX86MachineInstrPrinterPass(*PassRegistry::getPassRegistry());
    }

    bool runOnMachineFunction(MachineFunction &MF) override;

    StringRef getPassName() const override { return X86_MACHINEINSTR_PRINTER_PASS_NAME; }
};

} // end of anonymous namespace

char X86MachineInstrPrinter::ID = 0;

bool X86MachineInstrPrinter::runOnMachineFunction(MachineFunction &MF) {

    for (auto &MBB : MF) {
        const BasicBlock *BB = MBB.getBasicBlock();
        for (auto &MI : MBB) {
            switch (MI.getOpcode()) {
                case X86::RET16:
                case X86::RET32:
                case X86::RET64:
                case X86::RETI16:
                case X86::RETI32:
                case X86::RETI64: {

                    // Figure out the function we are returning to
                    // const Function *F = BB->getParent();
                    // outs() << "Found a return instruction in function " << F->getName() << "\n";


                    outs() << "Found a return instruction!\n";
                    outs() << "Contents of MachineInstr:\n";
                    outs() << MI << "\n";
                    for (auto &Op : MI.operands()) {
                        outs() << "Operand: " << Op << "\n";
                    }
                    break;
                }
            }
        }
    }

    return false;
}


INITIALIZE_PASS(X86MachineInstrPrinter, "x86-machineinstr-printer",
    X86_MACHINEINSTR_PRINTER_PASS_NAME,
    true, // is CFG only?
    true  // is analysis?
)

FunctionPass *llvm::createX86MachineInstrPrinter() { 
  return new X86MachineInstrPrinter(); 
}
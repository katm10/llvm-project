#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Pass.h"

using namespace llvm;

#define X86_MACHINEINSTR_PRINTER_PASS_NAME "Dummy X86 machineinstr printer pass"

namespace {

/*
  0: Don't apply this pass.
  1: Apply this pass and translate return addresses.
  2: Apply this pass, but don't translate return addresses. Just add no-ops for padding.
*/
cl::opt<int> TranslationMode(
    "return-addr-translation", 
    cl::desc("Return address translation mode"),
    cl::init(0));

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

    // TODO: this is kinda a hack
    if (MF.getFunction().getName().endswith("_addr")) {
        return false;
    }

    if (TranslationMode == 0) {
        return false;
    }

    bool modified = false;
    for (auto &MBB : MF) {
        const BasicBlock *BB = MBB.getBasicBlock();
        for (auto inst_iter = MBB.begin(); inst_iter != MBB.end(); inst_iter++) {
            MachineInstr &MI = *inst_iter;
            switch (MI.getOpcode()) {
                case X86::RET16:
                case X86::RET32:
                case X86::RET64:
                case X86::RETI16:
                case X86::RETI32:
                case X86::RETI64: {
                    const X86Subtarget &STI = MF.getSubtarget<X86Subtarget>();
                    const TargetInstrInfo &TII = *STI.getInstrInfo();

                    if (TranslationMode == 1) {
                      MCSymbol *translation_fxn = MF.getContext().getOrCreateSymbol(Twine("translate_return"));
                      translation_fxn->setExternal(true);

                      // jump (rip)translate_return
                      MachineInstrBuilder MIB = BuildMI(MBB, inst_iter, MI.getDebugLoc(), TII.get(X86::JMP_1))
                          .addSym(translation_fxn);
                    } else if (TranslationMode == 2) {
                        // TODO: figure out the correct amount of padding, maybe dynamically? 
                        // int padding = 8;

                        // Make a copy of the return instr
                        MachineInstrBuilder MIB = BuildMI(MBB, inst_iter, MI.getDebugLoc(), TII.get(MI.getOpcode()));

                        // for (int i = 0; i < padding -2; i++) {
                        //     MachineInstrBuilder MIB = BuildMI(MBB, inst_iter, MI.getDebugLoc(), TII.get(X86::NOOP));
                        // }
                    } else {
                        outs() << "Invalid translation mode!\n";
                        return false;
                    }

                    modified = true;
                }
            }
        }
    }

    return modified;
}


INITIALIZE_PASS(X86MachineInstrPrinter, "x86-machineinstr-printer",
    X86_MACHINEINSTR_PRINTER_PASS_NAME,
    true, // is CFG only?
    true  // is analysis?
)

FunctionPass *llvm::createX86MachineInstrPrinter() { 
  return new X86MachineInstrPrinter(); 
}
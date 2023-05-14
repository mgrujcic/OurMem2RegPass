#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
  struct OurMem2RegPass : public FunctionPass {
    static char ID;
    OurMem2RegPass() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      errs() << "Zdravo iz " << F.getName() << "\n";
      return true;
    }    
  };
}

char OurMem2RegPass::ID = 0;
static RegisterPass<OurMem2RegPass> Z("ourmem2reg", "Our mem2reg pass");
 //
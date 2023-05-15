#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include <string>
#include <unordered_map>
#include <vector>
#include "DomTree.h"
#include "llvm/Analysis/DominanceFrontier.h"

using namespace llvm;

namespace {
  struct OurMem2RegPass : public FunctionPass {
    static char ID;
    OurMem2RegPass() : FunctionPass(ID) {}


    bool runOnFunction(Function &F) override {

      DomTree domTree = DomTree(F);
      auto dominanceFrontier = domTree.GetDominanceFrontiers();
      
      for(auto P: dominanceFrontier){
        errs() << P.first->getName() << "\n";
        for(auto BB: P.second){
          errs() << "    " << BB->getName() << "\n";
        }
      }
      return true;
    }
  };
}

char OurMem2RegPass::ID = 0;
static RegisterPass<OurMem2RegPass> Z("ourmem2reg", "Our mem2reg pass");

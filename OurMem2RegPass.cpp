#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include <string>
#include <map>
#include <vector>
#include "DomTree.h"
#include "llvm/Analysis/DominanceFrontier.h"

using namespace llvm;

namespace {
  struct OurMem2RegPass : public FunctionPass {
    static char ID;
    OurMem2RegPass() : FunctionPass(ID) {}

    std::map<AllocaInst*, std::vector<Instruction*>> getAllocaVariableMapping(Function &F){

      std::map<AllocaInst*, std::vector<Instruction*>> variableMapping;

      for(auto BasicBlockIter = F.begin(); BasicBlockIter != F.end(); BasicBlockIter++){
        for(auto InstructionIter = BasicBlockIter->begin(); InstructionIter != BasicBlockIter->end(); InstructionIter++){

          Instruction *ins = &*InstructionIter;
          if(AllocaInst *allocaIns = dyn_cast<AllocaInst>(ins)){
            variableMapping[allocaIns] = std::vector<Instruction *>();
            //iterate through all uses of the register that stores the value given by alloca and add them to the map
            for(User *x: ins->users()){
              if(Instruction *Inst = dyn_cast<Instruction>(x)){
                variableMapping[allocaIns].push_back(Inst);
              }
            }
          }
        }
      }
      return variableMapping;
    }


    bool runOnFunction(Function &F) override {
      errs() << "Zdravo iz " << F.getName() << "\n";
      auto mapping = getAllocaVariableMapping(F);

      DomTree domTree = DomTree(F);
      auto dominanceFrontier = domTree.GetDominanceFrontiers();
      
      for(auto P: dominanceFrontier){
        errs() << P.first->getName() << "\n";
        for(auto BB: P.second){
          errs() << "    " << BB->getName() << "\n";
        }
      }

      /* llvm analysis for dominance frontier, used for testing
      auto nes = DominanceFrontier();
      nes.analyze(duci.tree);
      nes.dump();
      */

      // dominanceFrontier.viewGraph();
      // F.viewCFG();

      return true;
    }
  };
}

char OurMem2RegPass::ID = 0;
static RegisterPass<OurMem2RegPass> Z("ourmem2reg", "Our mem2reg pass");

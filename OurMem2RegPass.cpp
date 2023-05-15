#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Dominators.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "DomTree.h"
#include "llvm/Analysis/DominanceFrontier.h"

using namespace llvm;

namespace {
  struct OurMem2RegPass : public FunctionPass {
    static char ID;
    OurMem2RegPass() : FunctionPass(ID) {}

    /* InsertPhiInstructions
    Cytron et al. section 5.1

    Inserts phi instructions to the beginnings of basic blocks in accordance to the algorithm described in the paper

    IN:
    F - Current function
    blocksWithStores - For each variable what are the basic blocks where there is an assignment to it
    DomFrontiers - Dominance frontiers
    
    OUT:
    PhiToVariableMapping - For each of the phi instructions inserted, for which variable is that phi node used for
    
    */
    void InsertPhiInstructions(Function &F,
                               std::vector<Value *> &allVariables,
                               std::unordered_map<Value *, std::unordered_set<BasicBlock *>> &blocksWithStores,
                               std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> &DomFrontiers,
                               std::unordered_map<PHINode *, Value*> &PhiToVariableMapping){
      int iterCount = 0;
      std::unordered_map <BasicBlock*, int> HasAlready, Work;
      for(auto BBIterator = F.begin(); BBIterator != F.end(); BBIterator++) {
        HasAlready[&*BBIterator] = 0;
        Work[&*BBIterator] = 0;
      }

      std::unordered_set<BasicBlock *> Worklist;
      for(Value* variableValue: allVariables){
        iterCount++;
        for(auto BB: blocksWithStores[variableValue]){
          Work[BB] = iterCount;
          Worklist.insert(BB);
        }

        while(!Worklist.empty()){
          BasicBlock* currentBlock = *Worklist.begin();
          Worklist.erase(Worklist.begin());

          for(auto DFBlock: DomFrontiers[currentBlock]){
            if(HasAlready[DFBlock] < iterCount){
              /// ADD THE PHI INSTRUCTION ///
              // auto currentPHINode = PHINode::Create(variableValue->getType(),
              //                 pred_size(DFBlock),
              //                 variableValue->getName() + "_in_" + DFBlock->getName(),
              //                 &DFBlock->front());
              // PhiToVariableMapping[currentPHINode] = variableValue;
              ///////////////////////////////
              errs() << "Should add a phi instruction to block " << DFBlock->getName() << " for variable " << variableValue->getName() << "\n";
              HasAlready[DFBlock] = iterCount;
              if(Work[DFBlock] < iterCount){
                Work[DFBlock] = iterCount;
                Worklist.insert(DFBlock);
              }
            }
          }
        }
      }
    }

    /*getVariables

    IN:
    F - current Function

    OUT:
    allVariables - all Alloca instructions
    blocksWithStores - for each variable lists all of the basic blocks where there is a store instruction to it
    */
    void getVariables(Function &F,
                      std::vector<Value *> &allVariables, 
                      std::unordered_map<Value *, 
                      std::unordered_set<BasicBlock *>> &blocksWithStores){
      
      for(auto BasicBlockIter = F.begin(); BasicBlockIter != F.end(); BasicBlockIter++){
        for(auto InstructionIter = BasicBlockIter->begin(); InstructionIter != BasicBlockIter->end(); InstructionIter++){

          Instruction *ins = &*InstructionIter;
          if(AllocaInst *allocaIns = dyn_cast<AllocaInst>(ins)){
            allVariables.push_back(allocaIns);
          }else if(StoreInst *storeIns = dyn_cast<StoreInst>(ins)){
            Value* targetVariable = storeIns->getPointerOperand();
            blocksWithStores[targetVariable].insert(&*BasicBlockIter);
          }
        }
      }
    }

    bool runOnFunction(Function &F) override {
      std::vector<Value *> allVariables;
      std::unordered_map<Value *, std::unordered_set<BasicBlock *>> blocksWithStores;

      getVariables(F, allVariables, blocksWithStores);

      DomTree domTree = DomTree(F);
      auto dominanceFrontier = domTree.GetDominanceFrontiers();

      std::unordered_map<PHINode *, Value*> PhiToVariableMapping;
      InsertPhiInstructions(F, allVariables, blocksWithStores, dominanceFrontier, PhiToVariableMapping);

      
      return true;
    }
  };
}

char OurMem2RegPass::ID = 0;
static RegisterPass<OurMem2RegPass> Z("ourmem2reg", "Our mem2reg pass");

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constant.h"
#include <unordered_set>
#include <stack>
#include "DomTree.h"

using namespace llvm;

namespace {
    struct OurMem2RegPass : public FunctionPass {
        static char ID;
        OurMem2RegPass() : FunctionPass(ID) {}


        
        /*getVariables

        IN:
        F - current Function

        OUT:
        allVariables - all Alloca instructions
        blocksWithStores - for each variable lists all of the basic blocks where there is a store instruction to it
        */

        void getVariables(Function &F,
                          std::unordered_set<AllocaInst *> &allVariables,
                          std::unordered_map<AllocaInst *, std::unordered_set<BasicBlock *>> &blocksWithStores)
        {

            for(auto &BB: F){
                for(auto &Ins: BB){

                    Instruction *ins = &Ins;
                    if(AllocaInst *allocaIns = dyn_cast<AllocaInst>(ins)){
                        
                        //1. if the alloca variable is used by any instruction that is not either load or store do not do anything with it
                        bool shouldBeSSA = true;
                        for(Value* varUse: allocaIns->users()){
                            
                            if(! (isa<LoadInst>(varUse) || isa<StoreInst>(varUse))){
                                shouldBeSSA = false;
                                break;
                            }
                        }
                        if(!shouldBeSSA)
                            continue;
			            
                        //2. add it to the list of variables
                        allVariables.insert(allocaIns);

                        //3. find all the stores to the variable
                        for(Value* varUse: allocaIns->users()){
                            if(auto storeIns = dyn_cast<StoreInst>(varUse)){
                                blocksWithStores[allocaIns].insert(storeIns->getParent());
                            }
                        }

                    }
                }
            }
        }


        /* InsertPhiInstructions
        Cytron et al. section 5.1

        Inserts phi instructions to the beginnings of basic blocks in accordance to the algorithm described in the paper

        IN:
        F - Current function
        blocksWithStores - For each variable what are the basic blocks where there is an assignment to it
        DomFrontiers - Dominance frontiers

        OUT:
        PhiToVariableMapping - For each of the phi instructions inserted, for which variable is that phi node used for
        OurPhiNodes - Since Clang can generate phi instructions in some cases, keep track of which of the instructions were created by this pass
        */
        void InsertPhiInstructions(Function &F,
                                   std::unordered_set<AllocaInst *> &allVariables,
                                   std::unordered_map<AllocaInst *, std::unordered_set<BasicBlock *>> &blocksWithStores,
                                   std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> &DomFrontiers,
                                   std::unordered_map<PHINode *, Value*> &PhiToVariableMapping,
                                   std::unordered_set<PHINode*> &OurPhiNodes)
        {
            int iterCount = 0;
            std::unordered_map <BasicBlock*, int> HasAlready, Work;
            for(auto &BB: F) {
                HasAlready[&BB] = 0;
                Work[&BB] = 0;
            }

            std::unordered_set<BasicBlock *> Worklist;
            for(AllocaInst* variableValue: allVariables){
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
                            //insert the phi instruction
                            errs() << "Should add a phi instruction to block " << DFBlock->getName() << " for variable " << variableValue->getName() << 
                                " current block " << currentBlock->getName() << "\n";
                            auto currentPHINode = PHINode::Create(variableValue->getAllocatedType(),
                                                                  pred_size(DFBlock),
                                                                  variableValue->getName() + "_in_" + DFBlock->getName(),
                                                                  &DFBlock->front());
                            PhiToVariableMapping[currentPHINode] = variableValue;
                            OurPhiNodes.insert(currentPHINode);
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

        /**
         *
         * @brief Prints every instruction of function F
         *
         * @param[in] F function
         */
        void printIR(const Function &F)
        {
            for(auto &BB: F) {
                errs() << BB.getName() << ":\n";
                for (auto &Instr: BB) {
                    errs() <<'\t';
                    Instr.print(errs());
                    errs() << '\n';
                }
            }
        }

        /**
         *
         * @param[in] BB basic block that is currently being iterated
         * @param[in] allVariables all alloca instructions
         * @param[in] PhiToVariableMapping maps phi instruction to the variable which it stores into
         * @param[in] domTree self explanatory
         * @param[in/out] VarUseStack a stack for each variable that represents what was last stored into it
         * @param[out] ToDelete instructions that should later be deleted
         * @param[in] OurPhiNodes set of our newly inserted phi nodes
         *
         */
        void renameVars(BasicBlock *BB, 
                        const std::unordered_set<AllocaInst *> &allVariables, 
                        std::unordered_map<PHINode *, Value*> &PhiToVariableMapping,
                        const DomTree &domTree,
                        std::unordered_map<Value *, std::stack<Value*>> &VarUseStack,
                        std::vector<Instruction*> &ToDelete,
                        const std::unordered_set<PHINode*> &OurPhiNodes)
        {
            for(Instruction &InstRef : *BB){
                Instruction *Inst = &InstRef;
                //unchecked dyn cast! should be allocainst, even if the cast fails it just would not find it in the set, so the condition is false
                if(isa<StoreInst>(Inst) && allVariables.find(dyn_cast<AllocaInst>(Inst->getOperand(1))) != allVariables.end()){//unchecked dyn
                    Value *storeSrc = Inst->getOperand(0);
                    Value *storeDest = Inst->getOperand(1);
                    VarUseStack[storeDest].push(storeSrc);
                }
                else if(isa<LoadInst>(Inst) && allVariables.find(dyn_cast<AllocaInst>(Inst->getOperand(0))) != allVariables.end()){//unchecked dyn
                    Value *loadVal = Inst->getOperand(0);
                    Inst->replaceAllUsesWith(VarUseStack[loadVal].top());
                }
                else if(PHINode *phi = dyn_cast<PHINode>(Inst)){
                    //if the phi instruction was not added by this pass skip it
                    if(OurPhiNodes.find(phi) == OurPhiNodes.end())
                        continue;
                    Value *val = PhiToVariableMapping[phi];
                    VarUseStack[val].push(Inst);
                }
            }

            for(auto Successor: successors(BB)){
                for(Instruction &InstRef: *Successor){
                    if(PHINode *phi = dyn_cast<PHINode>(&InstRef)){
                        //if the phi instruction was not added by this pass skip it
                        if(OurPhiNodes.find(phi) == OurPhiNodes.end())
                            continue;

                        AllocaInst *val = dyn_cast<AllocaInst>(PhiToVariableMapping[phi]);
                        Value *valForPhi = VarUseStack[val].empty()?Constant::getNullValue(val->getAllocatedType()):
                                                                    VarUseStack[val].top();
                        phi->addIncoming(valForPhi, BB);
                    }
                }
            }
            
            for(auto *Child: domTree.tree.getNode(BB)->children()){
                renameVars(Child->getBlock(), allVariables, PhiToVariableMapping, domTree, VarUseStack, ToDelete, OurPhiNodes);
            }

            for(Instruction &InstRef : *BB){
                Instruction *Inst = &InstRef;
                if(isa<StoreInst>(Inst) && allVariables.find(dyn_cast<AllocaInst>(Inst->getOperand(1))) != allVariables.end()){//unchecked dyn
                    Value *storeDest = Inst->getOperand(1);
                    VarUseStack[storeDest].pop();
                    ToDelete.push_back(Inst);
                }
                else if(isa<LoadInst>(Inst) && allVariables.find(dyn_cast<AllocaInst>(Inst->getOperand(0))) != allVariables.end()){//unchecked dyn
                    ToDelete.push_back(Inst);
                }
                else if(PHINode *phi = dyn_cast<PHINode>(Inst)){
                    //if the phi instruction was not added by this pass skip it
                     if(OurPhiNodes.find(phi) == OurPhiNodes.end())
                        continue;
                    Value *val = PhiToVariableMapping[phi];
                    VarUseStack[val].pop();
                }
            }
        }

        bool runOnFunction(Function &F) override
        {
            std::unordered_set<AllocaInst *> allVariables;
            std::unordered_map<AllocaInst *, std::unordered_set<BasicBlock *>> blocksWithStores;

            getVariables(F, allVariables, blocksWithStores);

            DomTree domTree = DomTree(F);
            auto dominanceFrontier = domTree.GetDominanceFrontiers();

            std::unordered_map<PHINode *, Value*> PhiToVariableMapping;
            std::unordered_set<PHINode*> OurPhiNodes;

            InsertPhiInstructions(F, allVariables, blocksWithStores, dominanceFrontier, PhiToVariableMapping, OurPhiNodes);

            std::unordered_map<Value *, std::stack<Value*>> VarUseStack;
            for(Value* v: allVariables) {
                VarUseStack[v] = std::stack<Value*>();
            }
            std::vector<Instruction*> ToDelete;

            renameVars(&F.getEntryBlock(), allVariables, PhiToVariableMapping, domTree, VarUseStack, ToDelete, OurPhiNodes);


            for(Instruction* Inst:ToDelete){
                Inst->eraseFromParent();
            }

            for(auto AllocaVal: allVariables){
                if(Instruction* AllocaInst = dyn_cast<Instruction> (AllocaVal)){
                    AllocaInst->eraseFromParent();
                }
            }

            return true;
        }
    };
}

char OurMem2RegPass::ID = 0;
static RegisterPass<OurMem2RegPass> Z("ourmem2reg", "Our mem2reg pass");

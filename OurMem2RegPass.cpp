#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Instructions.h"
#include <unordered_set>
#include <stack>
#include "DomTree.h"

using namespace llvm;

namespace {
    struct OurMem2RegPass : public FunctionPass {
        static char ID;
        OurMem2RegPass() : FunctionPass(ID) {}

        std::unordered_map<Value *, std::stack<Value*>> VarUseStack;
        std::vector<Instruction*> ToDelete;
        std::unordered_map<Value*, Type*> VarType;

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
                                   std::unordered_set<Value *> &allVariables,
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
                            auto currentPHINode = PHINode::Create(VarType[variableValue],
                                                                  pred_size(DFBlock),
                                                                  variableValue->getName() + "_in_" + DFBlock->getName(),
                                                                  &DFBlock->front());
                            PhiToVariableMapping[currentPHINode] = variableValue;
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

        //for debugging purposes
        void printIR(Function &F){
            for(auto BB = F.begin();BB!=F.end();BB++) {
                errs()<<BB->getName()<<":\n";
                for (auto Instr = BB->begin();Instr != BB->end();Instr++) {
                    Instr->print(errs());
                    errs() << "\n";
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
                          std::unordered_set<Value *> &allVariables,
                          std::unordered_map<Value *,
                                  std::unordered_set<BasicBlock *>> &blocksWithStores){

            for(auto BasicBlockIter = F.begin(); BasicBlockIter != F.end(); BasicBlockIter++){
                for(auto InstructionIter = BasicBlockIter->begin(); InstructionIter != BasicBlockIter->end(); InstructionIter++){

                    Instruction *ins = &*InstructionIter;
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
                        VarType[allocaIns] = allocaIns->getAllocatedType();
                        allVariables.insert(allocaIns);

                        //3. find all the stores to the variable
                        for(Value* varUse: allocaIns->users()){
                            if(auto storeIns = dyn_cast<StoreInst>(varUse)){
                                blocksWithStores[ins].insert(storeIns->getParent());
                            }
                        }

                    }
                }
            }
        }


        void renameVars(BasicBlock* BB, std::unordered_set<Value *> allVariables, std::unordered_map<PHINode *, Value*> PhiToVariableMapping, Function &F){
            for(auto InstructionIter = BB->begin(); InstructionIter != BB->end(); InstructionIter++){
                Instruction* Inst = &*InstructionIter;
                if(isa<StoreInst>(Inst)){
                    if(allVariables.find(Inst->getOperand(1))!=allVariables.end()) {
                        VarUseStack[Inst->getOperand(1)].push(Inst->getOperand(0));
                    }
                }
                else if(isa<LoadInst>(Inst)){
                    if(allVariables.find(Inst->getOperand(0))!=allVariables.end()){
                        Inst->replaceAllUsesWith(VarUseStack[Inst->getOperand(0)].top());
                    }
                }
                else if(isa<PHINode>(Inst)){
                    PHINode *phi = dyn_cast<PHINode>(Inst);
                    Value* val = PhiToVariableMapping[phi];
                    if(val){
                        VarUseStack[val].push(Inst);
                    }
                }
            }
            for(succ_iterator i = succ_begin(BB), e = succ_end(BB); i!=e; i++){
                for(Instruction &InstRef: **i){
                    PHINode *phi;
                    if(isa<PHINode>(&InstRef)){
                        phi= dyn_cast<PHINode>(&InstRef);
                        if(PhiToVariableMapping[phi]) {
                            phi->addIncoming(VarUseStack[PhiToVariableMapping[phi]].top(), BB);
                        }
                    }
                }
            }
            DomTree domTree = DomTree(F);
            for(auto *Child: domTree.tree.getNode(BB)->children()){
                renameVars(Child->getBlock(), allVariables, PhiToVariableMapping, F);
            }

            for(Instruction &InstRef : *BB){
                Instruction *Inst = &InstRef;
                if(isa<StoreInst>(Inst) && allVariables.find(Inst->getOperand(1))!=allVariables.end()){
                    VarUseStack[Inst->getOperand(1)].pop();
                    ToDelete.push_back(Inst);
                }
                else if(PHINode *phi = dyn_cast<PHINode>(Inst)){
                    if(PhiToVariableMapping[phi]){
                        VarUseStack[PhiToVariableMapping[phi]].pop();
                    }
                }
                else if(isa<LoadInst>(Inst) && allVariables.find(Inst->getOperand(0))!=allVariables.end()){
                    ToDelete.push_back(Inst);
                }
            }
        }

        bool runOnFunction(Function &F) override {
            std::unordered_set<Value *> allVariables;
            std::unordered_map<Value *, std::unordered_set<BasicBlock *>> blocksWithStores;

            getVariables(F, allVariables, blocksWithStores);

            DomTree domTree = DomTree(F);
            auto dominanceFrontier = domTree.GetDominanceFrontiers();

            std::unordered_map<PHINode *, Value*> PhiToVariableMapping;
            InsertPhiInstructions(F, allVariables, blocksWithStores, dominanceFrontier, PhiToVariableMapping);

            for(Value* v: allVariables) {
                VarUseStack[v] = std::stack<Value*>();
            }

            renameVars(&F.getEntryBlock(), allVariables, PhiToVariableMapping, F);

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

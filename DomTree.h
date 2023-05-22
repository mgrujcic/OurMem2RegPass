#ifndef OURMEM2REG_DOMTREE_H
#define OURMEM2REG_DOMTREE_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/CFG.h"
#include <unordered_map>

using namespace llvm;

class DomTree {
    private:
        Function *currentFunction;
        void DomFrontierDFS(DomTreeNode *, std::unordered_map<BasicBlock *, std::vector<BasicBlock *>>&);
    public:
        DominatorTree tree;
        DomTree(Function &);
        void viewGraph();
        std::unordered_map<BasicBlock *, std::vector<BasicBlock *>> GetDominanceFrontiers();
};

#endif
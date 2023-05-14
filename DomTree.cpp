#include "DomTree.h"

DomTree::DomTree(Function &F){
    tree = DominatorTree(F);
    currentFunction = &F;
}

void DomTree::DomFrontierDFS(DomTreeNode* currentNode, std::map<BasicBlock *, std::vector<BasicBlock *>>& DomFrontier){
  for(DomTreeNode *DomTreeChild: currentNode->children())
    DomFrontierDFS(DomTreeChild, DomFrontier);

  errs() << ": " << currentNode->getBlock()->getName() << "\n"; 
  DomFrontier[currentNode->getBlock()] = std::vector<BasicBlock *>();
  //Cytron et al. page 16 (466)
  for(auto CFGSuccessor : successors(currentNode->getBlock())){
    DomTreeNode* SuccessorDTNode = tree.getNode(CFGSuccessor);
    if(SuccessorDTNode->getIDom() != currentNode)
      DomFrontier[currentNode->getBlock()].push_back(CFGSuccessor);
  }

  for(DomTreeNode *DomTreeChild: currentNode->children()){
    for(BasicBlock *ChildDFBlock: DomFrontier[DomTreeChild->getBlock()]){
      DomTreeNode* ChildDFNode = tree.getNode(ChildDFBlock);
      if(ChildDFNode->getIDom() != currentNode)
        DomFrontier[currentNode->getBlock()].push_back(ChildDFBlock);
    }
  }
}

std::map<BasicBlock *, std::vector<BasicBlock *>>  DomTree::GetDominanceFrontiers(){
  std::map<BasicBlock *, std::vector<BasicBlock *> > DomFrontier;
  DomTreeNode* rootNode = tree.getRootNode();
  DomFrontierDFS(rootNode, DomFrontier);
  return DomFrontier;
}

void DomTree::viewGraph(){
  tree.viewGraph();
}
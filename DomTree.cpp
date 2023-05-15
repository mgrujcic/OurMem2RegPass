#include "DomTree.h"

DomTree::DomTree(Function &F){
    tree = DominatorTree(F);
    currentFunction = &F;
}
/*DomFrontierDFS
Cytron et al. section 4.2
DFS traversal of the the dominator tree, process nodes upon exiting

IN:
currentNode - current node in the traversal of the dominator tree

OUT:
DomFrontier - dominance frontier map
*/
void DomTree::DomFrontierDFS(DomTreeNode* currentNode, std::unordered_map<BasicBlock *, std::vector<BasicBlock *>>& DomFrontier){
  //traverse the children
  for(DomTreeNode *DomTreeChild: currentNode->children())
    DomFrontierDFS(DomTreeChild, DomFrontier);

  //initialize the list of nodes in the frontier of the current node to an empty set
  DomFrontier[currentNode->getBlock()] = std::vector<BasicBlock *>();
  
  //local
  for(auto CFGSuccessor : successors(currentNode->getBlock())){
    DomTreeNode* SuccessorDTNode = tree.getNode(CFGSuccessor);
    if(SuccessorDTNode->getIDom() != currentNode)
      DomFrontier[currentNode->getBlock()].push_back(CFGSuccessor);
  }

  //up
  for(DomTreeNode *DomTreeChild: currentNode->children()){
    for(BasicBlock *ChildDFBlock: DomFrontier[DomTreeChild->getBlock()]){
      DomTreeNode* ChildDFNode = tree.getNode(ChildDFBlock);
      if(ChildDFNode->getIDom() != currentNode)
        DomFrontier[currentNode->getBlock()].push_back(ChildDFBlock);
    }
  }
}

/*GetDominanceFrontier
Returns the dominance frontier map constructed by DomFrontierDFS
*/
std::unordered_map<BasicBlock *, std::vector<BasicBlock *>>  DomTree::GetDominanceFrontiers(){
  std::unordered_map<BasicBlock *, std::vector<BasicBlock *> > DomFrontier;
  DomTreeNode* rootNode = tree.getRootNode();
  DomFrontierDFS(rootNode, DomFrontier);
  return DomFrontier;
}

void DomTree::viewGraph(){
  tree.viewGraph();
}
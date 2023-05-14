add_llvm_library( LLVMOurMem2RegPass MODULE
  OurMem2RegPass.cpp
  DomTree.cpp
  PLUGIN_TOOL
  opt
  )
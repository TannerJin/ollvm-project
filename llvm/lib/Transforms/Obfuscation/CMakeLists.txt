add_llvm_library(LLVMObfuscation MODULE BUILDTREE_ONLY
	Utils.cpp
	CryptoUtils.cpp
    BogusControlFlow.cpp
    Flattening.cpp
    Substitution.cpp
    SplitBasicBlocks.cpp
    StringEncryption.cpp
    
    ADDITIONAL_HEADER_DIRS
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Obfuscation
    
    DEPENDS
    intrinsics_gen
    PLUGIN_TOOL
    opt
  )

# add_dependencies(LLVMObfuscation intrinsics_gen)

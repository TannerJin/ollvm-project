add_llvm_library(LLVMObfuscation
	Utils.cpp
	CryptoUtils.cpp
    BogusControlFlow.cpp
    Flattening.cpp
    Substitution.cpp
    SplitBasicBlocks.cpp
    FuncNameRecord.cpp
    AntiFishHook.cpp
    
    ADDITIONAL_HEADER_DIRS
    ${LLVM_MAIN_INCLUDE_DIR}/llvm/Transforms/Obfuscation
  )

add_dependencies(LLVMObfuscation intrinsics_gen)

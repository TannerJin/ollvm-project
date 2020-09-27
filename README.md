### OLLVM-Project

OLLVM based on  `/release/8.x` of LLVM

#### Install

> 1. `cd ollvm-project`
> 2. `mkdir build`
> 3. `cd build`
> 4. `cmake -G "Ninja" -DLLVM_ENABLE_PROJECTS="clang" -DCMAKE_BUILD_TYPE=MinSizeRel -DLLDB_USE_SYSTEM_DEBUGSERVER=YES -DLLVM_CREATE_XCODE_TOOLCHAIN=on -DCMAKE_INSTALL_PREFIX=~/Library/Developer/ ./../llvm`
> 5. `ninja`
> 6. `ninja install-xcode-toolchain`

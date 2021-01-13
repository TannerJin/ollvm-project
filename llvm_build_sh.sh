#!/bin/sh

#  llvm_build_sh.sh
#  StringTest
#
#  Created by jintao on 2021/1/12.
#  Copyright © 2021 jintao. All rights reserved.

llvmBuildFlag=$1
llvmProject=$2

# 编译工具链
function build_xcode_toolchain() {
    echo "Begin build xcode toolchain..."
    
    SRC_DIR=$PWD/$llvmProject
    BUILD_DIR=$PWD/$llvmProject-XcodeToolchain
    
    mkdir -p $BUILD_DIR && cd $_
    
    cmake -G "Ninja" \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DLLVM_APPEND_VC_REV=on \
    -DLLDB_USE_SYSTEM_DEBUGSERVER=YES \
    -DLLVM_CREATE_XCODE_TOOLCHAIN=on \
    -DCMAKE_INSTALL_PREFIX=~/Library/Developer/ \
    $SRC_DIR/llvm
    
    echo "Ninja build llvm..."
    ninja
    
    echo "Install xcode_toolchain..."
    ninja install-xcode-toolchain
}

# 编译clang
function build_clang() {
    echo "Begin build clang..."
    
    SRC_DIR=$PWD/$llvmProject
    BUILD_DIR=$PWD/$llvmProject-clang

    for arg; do
    case $arg in
    --src=*) SRC_DIR="${arg##*=}"; shift ;;
    --build=*) BUILD_DIR="${arg##*=}"; shift ;;
    *) echo "Incorrect usage." >&2; exit 1 ;;
    esac
    done

    echo
    echo "SRC_DIR . . . . = $SRC_DIR"
    echo "BUILD_DIR . . . = $BUILD_DIR"
    echo

    NINJA=$(xcrun -f ninja)

    HOST_COMPILER_PATH=$(dirname $(xcrun -f clang))

    mkdir -p $BUILD_DIR && cd $_
    
    set -x
    xcrun cmake -G Ninja \
    -DCMAKE_MAKE_PROGRAM=$NINJA \
    -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=ON \
    -DCMAKE_C_COMPILER=$HOST_COMPILER_PATH/clang \
    -DCMAKE_CXX_COMPILER=$HOST_COMPILER_PATH/clang++ \
    -DLLVM_TARGETS_TO_BUILD="X86" \
    -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" \
    -DLLDB_INCLUDE_TESTS=OFF \
    $SRC_DIR/llvm && $NINJA
}

# 编译Xcode
function build_xcode() {
    echo "Begin build xcode project..."
    
    SRC_DIR=$PWD/$llvmProject
    BUILD_DIR=$PWD/$llvmProject-Xcode
    
    mkdir -p $BUILD_DIR && cd $_
    
    cmake -G "Xcode" \
    $SRC_DIR/llvm
}

function build() {
    if [ $llvmBuildFlag = toolchain ]
    then
        build_xcode_toolchain
    elif [ $llvmBuildFlag == clang ]
    then
        build_clang
    elif [ $llvmBuildFlag == xcode ]
    then
        build_xcode
    else
        echo "无效的编译参数"
    fi
}

build

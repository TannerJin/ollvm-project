//
//  StringEncryption.cpp
//  LLVMObfuscation
//
//  Created by jintao on 2021/1/14.
//

/*
    @.str       ->       @symbol          ->    Instruction
    (constat)        (ConstantExpr)           (GetElementPtrInst)
    "string"            GEPExpr
 
 // 参考Swift Sring IR 实现, 生成str()方法
 func str() -> String {
     return "Hello, World!"
 }

 let gloadStr = str()
 
 */

#include "llvm/Transforms/Obfuscation/StringEncryption.h"
#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Constants.h"
//#include "llvm/IR/ConstantsContext.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/IRBuilder.h"

using namespace llvm;

namespace llvm {
    struct StringEncryption: ModulePass {
    public:
        static char ID;
        bool flag;
        
        StringEncryption() : ModulePass(ID) {}
        StringEncryption(bool flag): ModulePass(ID) {
            this->flag = flag;
        }
        
        bool runOnModule(Module &M) override;
        
        void replaceInstruction(Module &M, Instruction *oldIns, GlobalVariable *newVar, char key, size_t size);
        
        Instruction *generateInstruction(Module &M, Instruction *oldIns, GlobalVariable *newVar, char key, size_t size);
    };
}

char StringEncryption::ID = 0;

// opt
static RegisterPass<StringEncryption> X("strEncryption", "String Encryption");


bool StringEncryption::runOnModule(Module &M) {
//    M.dump();
    bool changed = false;
    
    for (GlobalVariable &Var : M.getGlobalList()) {

        if (Var.getName().str().substr(0, 4)==".str"
            && Var.isConstant()
            && Var.hasInitializer()
            && isa<ConstantDataSequential>(Var.getInitializer())
            && Var.getSection() != "llvm.metadata"
            )
        {
            Var.dump();
            // replace value
            ConstantDataArray *oldConst = dyn_cast<ConstantDataArray>(Var.getInitializer());
            StringRef oldStr = oldConst->getAsString();

            size_t size = oldStr.size();
            if (size <= 1) {  // "\0"
                continue;
            }
            
            char key = 3;
            SmallVector<Constant *, 8> elements;
            
            for (size_t i = 0; i<size-1; i++) {
                char origChar = oldStr[i];
                Constant *orig = ConstantInt::get(Type::getInt8Ty(M.getContext()), origChar);

                if ((origChar ^ key) == '\0') {
                    elements.push_back(orig);
                    continue;
                }
                
                Constant *keyConst = ConstantInt::get(Type::getInt8Ty(M.getContext()), key);
                Constant *newElement = ConstantExpr::getXor(orig, keyConst);
                elements.push_back(newElement);
            }

            Constant *lastElement = ConstantInt::get(Type::getInt8Ty(M.getContext()), '\0');
            elements.push_back(lastElement);
            
            ArrayType *newConstType = ArrayType::get(Type::getInt8Ty(M.getContext()), size);
            Constant *newConst = ConstantArray::get(newConstType, elements);
            
            oldConst->dump();
            newConst->dump();
            Var.setInitializer(newConst);
            
            // replace user
            for (User *user : Var.users()) {
                if (isa<ConstantExpr>(user)) {
                    user->dump();
                    for (User *_user : user->users()) {
                        if (isa<Instruction>(_user)) {
                            Instruction *Ins = dyn_cast<Instruction>(_user);
                            replaceInstruction(M, Ins, &Var, key, size);
                        } else {
                            continue;
                            _user->dump();
                            assert(false);
                            exit(1);
                        }
                    }
                } else if (isa<Instruction>(user)) {
                    Instruction *Ins = dyn_cast<Instruction>(user);
                    replaceInstruction(M, Ins, &Var, key, size);
                } else {
                    assert(false);
                    exit(1);
                }
            }
        }
    }
    
//    M.dump();
    errs() << "\nend\n";
    return changed;
}

void StringEncryption::replaceInstruction(Module &M, Instruction *oldIns, GlobalVariable *newVar, char key, size_t size) {
//    oldIns->setOperand(0, newConst);
    oldIns->dump();
    if (isa<ConstantExpr>(oldIns->getOperandList()[0])) {
        
        generateInstruction(M, oldIns, newVar, key, size);
        
//        oldIns->getOperandList()[0]->dump();
    }
}

/*
    for (int i = 0, i<size, ++i) {
        
    }
 */
// 解密逻辑
Instruction *StringEncryption::generateInstruction(Module &M, Instruction *oldIns, GlobalVariable *newVar, char key, size_t size) {
    Type *type = Type::getInt32Ty(M.getContext());
    Type *elementType = Type::getInt8Ty(M.getContext());

    BasicBlock *origBB = oldIns->getParent();
    BasicBlock *newBB = oldIns->getParent()->splitBasicBlock(oldIns);
    BasicBlock *initBB = BasicBlock::Create(M.getContext(), "initBB", oldIns->getFunction(), oldIns->getParent());
    BasicBlock *condBB = BasicBlock::Create(M.getContext(), "condBB", oldIns->getFunction(), oldIns->getParent());
    BasicBlock *trueBB = BasicBlock::Create(M.getContext(), "trueBB", oldIns->getFunction(), oldIns->getParent());
    BasicBlock *dataBB = BasicBlock::Create(M.getContext(), "dataBB", oldIns->getFunction(), oldIns->getParent());

    origBB->getTerminator()->eraseFromParent();  // remove br newBB

    IRBuilder<> origIRB(origBB);
    origIRB.CreateBr(initBB);
    
    // init
    IRBuilder<> initIRB(initBB);
    Value *initValue = ConstantInt::get(type, 0);
    Value *cmpValue = ConstantInt::get(type, size);
    AllocaInst *allocaValue = initIRB.CreateAlloca(type);
    initIRB.CreateStore(initValue, allocaValue);
    initIRB.CreateBr(condBB);
    
    // entry
    IRBuilder<> entryIRB(condBB);
    LoadInst *loadValue = entryIRB.CreateLoad(type, allocaValue);
    Value *icmp = entryIRB.CreateICmp(CmpInst::ICMP_ULT, loadValue, cmpValue);
    entryIRB.CreateCondBr(icmp, trueBB, newBB);
    
    // ifTrue(解密)
    IRBuilder<> trueIRB(trueBB);
    LoadInst *trueLoadValue = trueIRB.CreateLoad(type, allocaValue);
    
    Value *element = trueIRB.CreateInBoundsGEP(newVar, trueLoadValue);
    Value *xorValue = trueIRB.CreateXor(element, ConstantInt::get(elementType, key));
    
    Value *icmp2 = trueIRB.CreateICmp(CmpInst::ICMP_NE, xorValue, ConstantInt::get(elementType, '\0'));
    trueIRB.CreateCondBr(icmp2, dataBB, NULL);
    
    Value *trueNewValue = trueIRB.CreateBinOp(Instruction::Add, trueLoadValue, ConstantInt::get(type, 1));   // +1
    trueIRB.CreateStore(trueNewValue, allocaValue);
    trueIRB.CreateBr(condBB);
    
    // 解密数据处理
    IRBuilder<> dataIRB(dataBB);
    
    
    oldIns->getFunction()->dump();
}

// rdspy.cpp
// llvm pass
// Brandon Kammerdiener -- 2018

#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <stdio.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <vector>

using namespace llvm;

cl::opt<unsigned int>
    RDspySamplingThreshold("rdspy-sampling-threshold",
                 cl::desc("If >0, rdsyp will sample approximately every 1/threshold reads."),
                 cl::value_desc("threshold"));

static LLVMContext myContext;

namespace {

struct rdspy : public ModulePass {
    static char ID;

    rdspy() : ModulePass(ID) { }
    void getAnalysisUsage(AnalysisUsage & AU) const { }

    //////////////////////////////////////////////////////////////////// runOnModule
    ////////////////////////////////////////////////////////////////////////////////
    bool runOnModule(Module & module) {
        unsigned int thresh = RDspySamplingThreshold;

        auto & context = module.getContext();

        IRBuilder<> builder(context);        

        Type * i64_ty = builder.getInt64Ty();
        Type * ptr_ty = builder.getInt8PtrTy();

        std::vector<Type*> argTypes { ptr_ty, i64_ty, i64_ty };
        FunctionType * decl_ty = FunctionType::get(builder.getInt64Ty(), argTypes, false);
        Function * read_decl = Function::Create(decl_ty,
                                           GlobalValue::ExternalLinkage,
                                           "sh_read",
                                           &module);

        Function * readtime = Intrinsic::getDeclaration(&module, Intrinsic::readcyclecounter);

        std::vector<LoadInst*> reads;

        for (auto & F : module)
        for (auto & BB : F)
        for (auto it = BB.begin(); it != BB.end(); it++)
            if (LoadInst* load = dyn_cast<LoadInst>(&*it))
                reads.push_back(load);


        if (RDspySamplingThreshold) {
            // round to next power of 2
            thresh--;
            thresh |= thresh >> 1;
            thresh |= thresh >> 2;
            thresh |= thresh >> 4;
            thresh |= thresh >> 8;
            thresh |= thresh >> 16;
            thresh++;

            fprintf(stderr, "rdspy sample: %u\n", thresh);

            GlobalVariable * tls_counter = new GlobalVariable(
                    module,
                    i64_ty,
                    false,
                    GlobalValue::LinkOnceODRLinkage,
                    builder.getInt64(0),
                    "rdspy_read_counter",
                    nullptr,
                    GlobalValue::GeneralDynamicTLSModel); 

            GlobalVariable * tls_thresh = new GlobalVariable(
                    module,
                    i64_ty,
                    false,
                    GlobalValue::LinkOnceODRLinkage,
                    builder.getInt64(thresh / 2),
                    "rdspy_read_threshold",
                    nullptr,
                    GlobalValue::GeneralDynamicTLSModel); 

            for (LoadInst * read : reads) {
                Value * addr = read->getOperand(0);
                AllocaInst * alloca = dyn_cast<AllocaInst>(addr);

                if (!alloca) {
                    std::vector<Value*> no_args;

                    BasicBlock * BB = read->getParent();

                    /* load the counter and get ready to branch */
                    builder.SetInsertPoint(read);
                    Value * c     = builder.CreateLoad(tls_counter);
                    Value * t     = builder.CreateLoad(tls_thresh);
                    Value * cond  = builder.CreateICmpEQ(c, t);
                    /* ---------------------------------------- */

                    /* set up basic blocks */
                    BasicBlock * after_BB = BB->splitBasicBlock(cast<Instruction>(cond)->getNextNode());
                    BasicBlock * else_BB  = BasicBlock::Create(
                                                BB->getContext(),
                                                "rdspy_read_else",
                                                BB->getParent(),
                            /* insert before */ after_BB);
                    BasicBlock * then_BB  = BasicBlock::Create(
                                                BB->getContext(),
                                                "rdspy_read_then",
                                                BB->getParent(),
                            /* insert before */ else_BB);
                    /* ------------------- */


                    /* conditional branch */
                    Instruction * terminator = BB->getTerminator();
                    Instruction * br = BranchInst::Create(
                                            then_BB,
                                            else_BB,
                                            cond,
                     /* insert at end of */ BB);
                    (void)br;
                    terminator->eraseFromParent();
                    /* ------------------ */


                    /* The then block */
                    builder.SetInsertPoint(then_BB);

                    Value * beg = builder.CreateCall(readtime, no_args);
                    Value * then_read = builder.Insert(read->clone()); 
                    Value * end = builder.CreateCall(readtime, no_args);

                    Value * addr = builder.CreateBitCast(read->getOperand(0), ptr_ty);

                    std::vector<Value*> call_args { addr, beg, end };
                    Value * ret = builder.CreateCall(read_decl, call_args);
                    Value * hit = builder.CreateICmpEQ(ret, builder.getInt64(1));

                    // Take bits from the timer value to update the threshold
                    Value * old_thresh = t;
                    Value * new_thresh = builder.CreateAnd(end, builder.getInt64(thresh - 1));
                    new_thresh = builder.CreateSelect(hit, new_thresh, old_thresh);
                    builder.CreateStore(new_thresh, tls_thresh);
                  
                    Value * new_c = builder.CreateSelect(hit, builder.getInt64(0), c);
                    builder.CreateStore(new_c, tls_counter);

                    builder.CreateBr(after_BB);
                    /* -------------- */

                    /* The else block */
                    builder.SetInsertPoint(else_BB);

                    builder.CreateStore(builder.CreateAdd(c, builder.getInt64(1)), tls_counter);
                    Value * else_read = builder.Insert(read->clone());

                    builder.CreateBr(after_BB);
                    /* -------------- */

                    /* Merge the values and replace uses */
                    builder.SetInsertPoint(read);
                    PHINode * phi = PHINode::Create(read->getType(), 2);
                    phi->addIncoming(then_read, then_BB);
                    phi->addIncoming(else_read, else_BB);
                    ReplaceInstWithInst(read, phi);
                    /* --------------------------------- */
                }
            }
        } else {
            for (LoadInst * read : reads) {
                Value            *addr = read->getOperand(0);
                AllocaInst     *alloca = dyn_cast<AllocaInst>(addr);
                GlobalVariable *global = dyn_cast<GlobalVariable>(addr);

                /* Try to limit the addresses we look at to heap regions.
                 * Can't filter all non-heap, but this will help. */
                if (!alloca && !global) {
                    std::vector<Value*> no_args;

                    builder.SetInsertPoint(read);
                    Value * beg = builder.CreateCall(readtime, no_args);

                    builder.SetInsertPoint(read->getNextNode());
                    Value * end = builder.CreateCall(readtime, no_args);

                    Value * addr = builder.CreateBitCast(read->getOperand(0), ptr_ty);

                    std::vector<Value*> call_args { addr, beg, end };
                    builder.CreateCall(read_decl, call_args);
                }
            }
        }

        return true;
    }
    ////////////////////////////////////////////////////////////////////////////////
};

} // namespace
char rdspy::ID = 0;
static RegisterPass<rdspy> X("rdspy", "rdsypy Pass",
                             false /* Only looks at CFG */,
                             false /* Analysis Pass */);

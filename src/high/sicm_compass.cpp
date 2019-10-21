// compass.cpp
// llvm pass
// Brandon Kammerdiener -- 2018

#include <llvm/Config/llvm-config.h>
#if LLVM_VERSION_MAJOR >= 4
/* Required for CompassQuickExit, requires LLVM 4.0 or newer */
#include "llvm/Bitcode/BitcodeWriter.h"
#endif
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#if 0
#include "llvm/Support/Error.h"
#endif
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

// Input/output files
#define SOURCE_LOCATIONS_FILE "contexts.txt"
#define NSITES_FILE "nsites.txt"
#define NCLONES_FILE "nclones.txt"
#define BOTTOM_UP_CALL_GRAPH_FILE "buCG.txt"

using namespace llvm;

cl::opt<unsigned int>
    CompassDepth("compass-depth",
                 cl::desc("Specify context depth if in 'transform' mode"),
                 cl::value_desc("depth"));

cl::opt<std::string>
    CompassMode("compass-mode",
                cl::desc("Select the operation that compass performs. "
                         "'analyze' creates the bottom-up call graph file. "
                         "'transform' clones functions to resolve context."),
                cl::value_desc("mode"));

static cl::opt<bool>
    CompassQuickExit("compass-quick-exit", cl::Hidden,
                     cl::desc("Skip opt finalizations and exit immediately "
                              "after compass completes."));

static cl::opt<bool>
    CompassClassicalCG("compass-classical-cg", cl::Hidden,
                     cl::desc("Use a classical call graph rather than "
                              "an extended call graph."));
static cl::opt<bool>
    CompassDetail("compass-detail", cl::Hidden,
                     cl::desc("Emit more detail about the calling contexts "
                              "to the contexts file."));

__attribute__((used))
static void plv(llvm::Value *v) {
  v->print(llvm::outs());
}

static LLVMContext myContext;

namespace {

// Open a file that can be appended to simultaneously by multiple
// processes in a safe way.
FILE * sync_fopen(char * path) {
    FILE * f = fopen(path, "a");
    if (f) {
        setvbuf(f, NULL, _IONBF, BUFSIZ);
    }
    return f;
}

struct compass : public ModulePass {
    static char ID;

    unsigned int ncloned;
    std::map<Function *, std::set<Function *>> buCG;
    std::map<std::string, std::set<std::string>> str_buCG;
    std::map<std::string, std::set<std::string>> str_CG;
    std::map<std::string, std::string> cloneLink;
    std::map<std::string, unsigned int> sitesPerFn;
    std::map<std::string, unsigned int> times1Calls2;
    std::map<std::string, unsigned int> namecount;
    unsigned long long n_sites, pre_n_sites;
    Module * theModule;
    std::map<std::string, std::string> allocFnMap;
    std::map<std::string, std::string> dallocFnMap;

    FILE * contexts_file,
         * nsites_file,
         * nclones_file;

    compass() :
        ModulePass(ID),
        ncloned(0),
        n_sites(0),
        pre_n_sites(0),
        theModule(nullptr),
        contexts_file(nullptr),
        nsites_file(nullptr),
        nclones_file(nullptr) {

	/* C */
        allocFnMap["malloc"] = "sh_alloc";
        allocFnMap["calloc"] = "sh_calloc";
        allocFnMap["realloc"] = "sh_realloc";
        allocFnMap["aligned_alloc"] = "sh_aligned_alloc";
        allocFnMap["posix_memalign"] = "sh_posix_memalign";
        allocFnMap["memalign"] = "sh_memalign";
        dallocFnMap["free"] = "sh_free";

	/* C++ */
        allocFnMap["_Znam"] = "sh_alloc";
        allocFnMap["_Znwm"] = "sh_alloc";
        dallocFnMap["_ZdaPv"] = "sh_free";
        dallocFnMap["_ZdlPv"] = "sh_free";

	/* Fortran */
        allocFnMap["f90_alloc"] = "f90_sh_alloc";
        allocFnMap["f90_alloca"] = "f90_sh_alloca";
        allocFnMap["f90_alloc03"] = "f90_sh_alloc03";
        allocFnMap["f90_alloc03a"] = "f90_sh_alloc03a";
        allocFnMap["f90_alloc03_chk"] = "f90_sh_alloc03_chk";
        allocFnMap["f90_alloc03_chka"] = "f90_sh_alloc03_chka";
        allocFnMap["f90_alloc04"] = "f90_sh_alloc04";
        allocFnMap["f90_alloc04a"] = "f90_sh_alloc04a";
        allocFnMap["f90_alloc04_chk"] = "f90_sh_alloc04_chk";
        allocFnMap["f90_alloc04_chka"] = "f90_sh_alloc04_chka";
        allocFnMap["f90_kalloc"] = "f90_sh_kalloc";
        allocFnMap["f90_calloc"] = "f90_sh_calloc";
        allocFnMap["f90_calloc03"] = "f90_sh_calloc03";
        allocFnMap["f90_calloc03a"] = "f90_sh_calloc03a";
        allocFnMap["f90_calloc04"] = "f90_sh_calloc04";
        allocFnMap["f90_calloc04a"] = "f90_sh_calloc04a";
        allocFnMap["f90_kcalloc"] = "f90_sh_kcalloc";
        allocFnMap["f90_ptr_alloc"] = "f90_sh_ptr_alloc";
        allocFnMap["f90_ptr_alloca"] = "f90_sh_ptr_alloca";
        allocFnMap["f90_ptr_alloc03"] = "f90_sh_ptr_alloc03";
        allocFnMap["f90_ptr_alloc03a"] = "f90_sh_ptr_alloc03a";
        allocFnMap["f90_ptr_alloc04"] = "f90_sh_ptr_alloc04";
        allocFnMap["f90_ptr_alloc04a"] = "f90_sh_ptr_alloc04a";
        allocFnMap["f90_ptr_src_alloc03"] = "f90_sh_ptr_src_alloc03";
        allocFnMap["f90_ptr_src_alloc03a"] = "f90_sh_ptr_src_alloc03a";
        allocFnMap["f90_ptr_src_alloc04"] = "f90_sh_ptr_src_alloc04";
        allocFnMap["f90_ptr_src_alloc04a"] = "f90_sh_ptr_src_alloc04a";
        allocFnMap["f90_ptr_src_calloc03"] = "f90_sh_ptr_src_calloc03";
        allocFnMap["f90_ptr_src_calloc03a"] = "f90_sh_ptr_src_calloc03a";
        allocFnMap["f90_ptr_src_calloc04"] = "f90_sh_ptr_src_calloc04";
        allocFnMap["f90_ptr_src_calloc04a"] = "f90_sh_ptr_src_calloc04a";
        allocFnMap["f90_ptr_kalloc"] = "f90_sh_ptr_kalloc";
        allocFnMap["f90_ptr_calloc"] = "f90_sh_ptr_calloc";
        allocFnMap["f90_ptr_calloc03"] = "f90_sh_ptr_calloc03";
        allocFnMap["f90_ptr_calloc03a"] = "f90_sh_ptr_calloc03a";
        allocFnMap["f90_ptr_calloc04"] = "f90_sh_ptr_calloc04";
        allocFnMap["f90_ptr_calloc04a"] = "f90_sh_ptr_calloc04a";
        allocFnMap["f90_ptr_kcalloc"] = "f90_sh_ptr_kcalloc";
        allocFnMap["f90_auto_allocv"] = "f90_sh_auto_allocv";
        allocFnMap["f90_auto_alloc"] = "f90_sh_auto_alloc";
        allocFnMap["f90_auto_alloc04"] = "f90_sh_auto_alloc04";
        allocFnMap["f90_auto_calloc"] = "f90_sh_auto_calloc";
        allocFnMap["f90_auto_calloc04"] = "f90_sh_auto_calloc04";
        allocFnMap["f90_alloc_i8"] = "f90_sh_alloc_i8";
        allocFnMap["f90_alloca_i8"] = "f90_sh_alloca_i8";
        allocFnMap["f90_alloc03_i8"] = "f90_sh_alloc03_i8";
        allocFnMap["f90_alloc03a_i8"] = "f90_sh_alloc03a_i8";
        allocFnMap["f90_alloc03_chk_i8"] = "f90_sh_alloc03_chk_i8";
        allocFnMap["f90_alloc03_chka_i8"] = "f90_sh_alloc03_chka_i8";
        allocFnMap["f90_alloc04_i8"] = "f90_sh_alloc04_i8";
        allocFnMap["f90_alloc04a_i8"] = "f90_sh_alloc04a_i8";
        allocFnMap["f90_alloc04_chk_i8"] = "f90_sh_alloc04_chk_i8";
        allocFnMap["f90_alloc04_chka_i8"] = "f90_sh_alloc04_chka_i8";
        allocFnMap["f90_kalloc_i8"] = "f90_sh_kalloc_i8";
        allocFnMap["f90_calloc_i8"] = "f90_sh_calloc_i8";
        allocFnMap["f90_calloc03_i8"] = "f90_sh_calloc03_i8";
        allocFnMap["f90_calloc03a_i8"] = "f90_sh_calloc03a_i8";
        allocFnMap["f90_calloc04_i8"] = "f90_sh_calloc04_i8";
        allocFnMap["f90_calloc04a_i8"] = "f90_sh_calloc04a_i8";
        allocFnMap["f90_kcalloc_i8"] = "f90_sh_kcalloc_i8";
        allocFnMap["f90_ptr_alloc_i8"] = "f90_sh_ptr_alloc_i8";
        allocFnMap["f90_ptr_alloca_i8"] = "f90_sh_ptr_alloca_i8";
        allocFnMap["f90_ptr_alloc03_i8"] = "f90_sh_ptr_alloc03_i8";
        allocFnMap["f90_ptr_alloc03a_i8"] = "f90_sh_ptr_alloc03a_i8";
        allocFnMap["f90_ptr_alloc04_i8"] = "f90_sh_ptr_alloc04_i8";
        allocFnMap["f90_ptr_alloc04a_i8"] = "f90_sh_ptr_alloc04a_i8";
        allocFnMap["f90_ptr_src_alloc03_i8"] = "f90_sh_ptr_src_alloc03_i8";
        allocFnMap["f90_ptr_src_alloc03a_i8"] = "f90_sh_ptr_src_alloc03a_i8";
        allocFnMap["f90_ptr_src_alloc04_i8"] = "f90_sh_ptr_src_alloc04_i8";
        allocFnMap["f90_ptr_src_alloc04a_i8"] = "f90_sh_ptr_src_alloc04a_i8";
        allocFnMap["f90_ptr_src_calloc03_i8"] = "f90_sh_ptr_src_calloc03_i8";
        allocFnMap["f90_ptr_src_calloc03a_i8"] = "f90_sh_ptr_src_calloc03a_i8";
        allocFnMap["f90_ptr_src_calloc04_i8"] = "f90_sh_ptr_src_calloc04_i8";
        allocFnMap["f90_ptr_src_calloc04a_i8"] = "f90_sh_ptr_src_calloc04a_i8";
        allocFnMap["f90_ptr_kalloc_i8"] = "f90_sh_ptr_kalloc_i8";
        allocFnMap["f90_ptr_calloc_i8"] = "f90_sh_ptr_calloc_i8";
        allocFnMap["f90_ptr_calloc03_i8"] = "f90_sh_ptr_calloc03_i8";
        allocFnMap["f90_ptr_calloc03a_i8"] = "f90_sh_ptr_calloc03a_i8";
        allocFnMap["f90_ptr_calloc04_i8"] = "f90_sh_ptr_calloc04_i8";
        allocFnMap["f90_ptr_calloc04a_i8"] = "f90_sh_ptr_calloc04a_i8";
        allocFnMap["f90_ptr_kcalloc_i8"] = "f90_sh_ptr_kcalloc_i8";
        allocFnMap["f90_auto_allocv_i8"] = "f90_sh_auto_allocv_i8";
        allocFnMap["f90_auto_alloc_i8"] = "f90_sh_auto_alloc_i8";
        allocFnMap["f90_auto_alloc04_i8"] = "f90_sh_auto_alloc04_i8";
        allocFnMap["f90_auto_calloc_i8"] = "f90_sh_auto_calloc_i8";
        allocFnMap["f90_auto_calloc04_i8"] = "f90_sh_auto_calloc04_i8";
        dallocFnMap["f90_dealloc"] = "f90_sh_dealloc";
        dallocFnMap["f90_dealloca"] = "f90_sh_dealloca";
        dallocFnMap["f90_dealloc03"] = "f90_sh_dealloc03";
        dallocFnMap["f90_dealloc03a"] = "f90_sh_dealloc03a";
        dallocFnMap["f90_dealloc_mbr"] = "f90_sh_dealloc_mbr";
        dallocFnMap["f90_dealloc_mbr03"] = "f90_sh_dealloc_mbr03";
        dallocFnMap["f90_dealloc_mbr03a"] = "f90_sh_dealloc_mbr03a";
        dallocFnMap["f90_deallocx"] = "f90_sh_deallocx";
        dallocFnMap["f90_auto_dealloc"] = "f90_sh_auto_dealloc";
        dallocFnMap["f90_dealloc_i8"] = "f90_sh_dealloc_i8";
        dallocFnMap["f90_dealloca_i8"] = "f90_sh_dealloca_i8";
        dallocFnMap["f90_dealloc03_i8"] = "f90_sh_dealloc03_i8";
        dallocFnMap["f90_dealloc03a_i8"] = "f90_sh_dealloc03a_i8";
        dallocFnMap["f90_dealloc_mbr_i8"] = "f90_sh_dealloc_mbr_i8";
        dallocFnMap["f90_dealloc_mbr03_i8"] = "f90_sh_dealloc_mbr03_i8";
        dallocFnMap["f90_dealloc_mbr03a_i8"] = "f90_sh_dealloc_mbr03a_i8";
        dallocFnMap["f90_deallocx_i8"] = "f90_sh_deallocx_i8";
        dallocFnMap["f90_auto_dealloc_i8"] = "f90_sh_auto_dealloc_i8";
    }

    void getAnalysisUsage(AnalysisUsage & AU) const { }

    ///////////// Make the use of CallSite generic across CallInst and InvokeInst
    /////////////////////////////////////////////////////////////////////////////
    bool isCallSite(llvm::Instruction * inst) {
        return isa<CallInst>(inst) || isa<InvokeInst>(inst);
    }

    Function * getDirectlyCalledFunction(CallSite & site) {
        Function * fn  = nullptr;

        CallInst * call = dyn_cast<CallInst>(site.getInstruction());
        if (call) {
            fn = call->getCalledFunction();
        } else {
            InvokeInst * inv = dyn_cast<InvokeInst>(site.getInstruction());
            fn = inv->getCalledFunction();
        }

        return fn;
    }
    
    Value * getCalledValue(CallSite & site) {
        Value    * val = nullptr;

        CallInst * call = dyn_cast<CallInst>(site.getInstruction());
        if (call) {
            val = call->getCalledValue();
        } else {
            InvokeInst * inv = dyn_cast<InvokeInst>(site.getInstruction());
            val = inv->getCalledValue();
        }

        return val;
    }

    // Try to return the function that that a given call site calls.
    // If the call is direct this is easy.
    // If the call is indirect, we may still be able to get the function
    // if it is known statically.
    Function * getCalledFunction(CallSite & site) {
        Function * fn  = nullptr;
        Value    * val = nullptr;

        fn = getDirectlyCalledFunction(site);
        if (fn)
            return fn;

        // At this point, we are looking at an indirect call (common in code 
        // generated by flang).
        // We should still try to find the function if it is a simple bitcast:
        //
        //      %0 = bitcast void (...)* @f90_alloc04_chk to
        //              void (i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i64, ...)*
        //
        // So, we will try to scan through bitcasts.
        //
        // @incomplete: are there other operations that would make a statically direct
        // call look indirect?

        val = getCalledValue(site);

        if (BitCastOperator * op = dyn_cast<BitCastOperator>(val))
            return dyn_cast<Function>(op->stripPointerCasts());

        return nullptr;
    }

    void setCalledFunction(CallSite & site, Function * fn) {
        CallInst * call = dyn_cast<CallInst>(site.getInstruction());
        if (call) {
            call->setCalledFunction(fn);
            return;
        }
        dyn_cast<InvokeInst>(site.getInstruction())->setCalledFunction(fn);
    }

    User::op_iterator arg_begin(CallSite & site) {
        CallInst * call = dyn_cast<CallInst>(site.getInstruction());
        if (call)
            return call->arg_begin();
        return dyn_cast<InvokeInst>(site.getInstruction())->arg_begin();
    }

    User::op_iterator arg_end(CallSite & site) {
        CallInst * call = dyn_cast<CallInst>(site.getInstruction());
        if (call)
            return call->arg_end();
        return dyn_cast<InvokeInst>(site.getInstruction())->arg_end();
    }
    ////////////////////////////////////////////////////////////////////////////////


    ///////////////////////////////////////////////// construct bottom-up call graph
    ////////////////////////////////////////////////////////////////////////////////
   
    void rec_search(Function * f, std::set<Function*>& visited) {
        if (!f)
            return;

        if (visited.find(f) != visited.end())
            return;
        visited.insert(f);

        buCG[f];

        for (auto & BB : *f) {
            for (auto it = BB.begin(); it != BB.end(); it++) {
                if (isCallSite(&*it)) {
                    CallSite site(&*it);
                    Function * callee = getCalledFunction(site);
                    if (callee) {
                        std::string combined = f->getName().str() + callee->getName().str();
                        times1Calls2[combined] += 1;
                        buCG[callee].insert(f);
                        rec_search(callee, visited);
                    }
                }
            }
        }
    }
    
    void buildBottomUpCG() {
        std::set<Function *> visited;


        for (Function & node : *theModule) {
            rec_search(&node, visited);
        }
    }

    ////////////////////////////////////////////////////////////////////////////////


    /////////////////////////////////////////////////// Writing and reading buCG.txt
    ////////////////////////////////////////////////////////////////////////////////
    void writeBottomUpCG(std::ostream & stream) {
        stream << buCG.size();
        for (auto & it : buCG) {
            Function * callee = it.first;

            stream << " " << callee->getName().str();

            int n_sites = 0;
            if (!callee->isDeclaration()) {
                for (auto & BB : *callee) {
                    for (auto it = BB.begin(); it != BB.end(); it++) {
                        if (isCallSite(&*it)) {
                            CallSite site(&*it);
                            Function * called = getCalledFunction(site);
                            if (called) {
                                std::string name = called->getName().str();
                                if (allocFnMap.find(name) != allocFnMap.end())
                                    n_sites += 1;
                            }
                        }
                    }
                }
            }
            stream << " " << n_sites;
            stream << " " << it.second.size();
            for (Function * caller : it.second) {
                std::string combined = caller->getName().str() + callee->getName().str();
                stream << " " << caller->getName().str() << " " << times1Calls2[combined];
            }
            stream << "\n";
        }
    }

    // Remove edges to self.
    void filterRecursion() {
        for (auto & node : str_buCG)
            if (node.second.find(node.first) != node.second.end())
                node.second.erase(node.first);
    }

    void readBottomUpCG(std::istream & stream) {
        int len;
        stream >> len;
        for (int i = 0; i < len; i += 1) {
            std::string callee, caller;
            stream >> callee;
            unsigned int n_sites, n_callers;
            stream >> n_sites;
            sitesPerFn[callee] = n_sites;
            cloneLink[callee] = callee;
            stream >> n_callers;
            for (int j = 0; j < n_callers; j += 1) {
                int times_called;
                stream >> caller;
                str_buCG[callee].insert(caller);
                stream >> times_called;
                times1Calls2[caller+callee] = times_called;
            }
        }

        filterRecursion();
    }

    // Create the regular top-down call graph from the
    // bottom up one.
    void flipCG() {
        for (auto & node : str_buCG) {
            for (auto & caller : node.second)
                str_CG[caller].insert(node.first);
        }
    }
    ////////////////////////////////////////////////////////////////////////////////


    /////////////////////////////////////////////////////////////// Function cloning
    ////////////////////////////////////////////////////////////////////////////////
    std::string newCloneName(std::string name) {
        name += "__compass" + std::to_string(namecount[name]++);
        return name;
    }

    Function * createClone(Function * fn) {
        Function * fclone = nullptr;

        std::string name = newCloneName(fn->getName().str());

        if (fn->isDeclaration()) {
            fclone = Function::Create(cast<FunctionType>(fn->getValueType()),
                                      fn->getLinkage(), name, theModule);
            fclone->copyAttributesFrom(fn);
        } else {
            ValueToValueMapTy VMap;
#if (LLVM_VERSION_MAJOR <= 3 && LLVM_VERSION_MINOR <= 8)
            fclone = CloneFunction(fn, VMap, false);
#else
            fclone = CloneFunction(fn, VMap);
#endif
            fclone->setName(name);
        }

        if (fn->hasComdat()) {
            fclone->setComdat(fn->getComdat());
        }

        return fclone;
    }
    ////////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////// doCloning
    ////////////////////////////////////////////////////////////////////////////////
    // This is where most of the work of compass takes place.
    // This function builds the "layers" of functions from the roots (malloc,
    // new, etc.) and works down the layers cloning functions for each of their
    // callers such that there is a unique path to the root within CompassDepth
    // layers of function calls.
    //
    // Example:
    //                                      +----+
    //                                 +----+main+----+
    //                                 |    +----+    |
    //                                +v+            +v+
    //                                |a+---+    +---+b|
    //                                +++    \  /    +++
    //                                 |      \/      |
    //                                 |      /\      |
    //                                +v+    /  \    +v+
    //                                |c<---+    +--->d|
    //                                +++            +++
    //                                 |      +-+     |
    //                                 +------>e<-----+
    //                                        +++
    //                                         |
    //                                         |
    //                                     +---v---+
    //                                     |malloc*|
    //                                     +-------+
    //
    // In this call graph, malloc is the root.
    // The problem is that there are 4 possible paths to malloc:
    //      main->a->c->e->malloc
    //      main->a->d->e->malloc
    //      main->b->c->e->malloc
    //      main->b->d->e->malloc
    //
    // doCloning() walks down the layers of the call paths to roots like malloc
    // and makes clones so that all paths are unique. In this example, it would
    // create a clone of c and a clone of d:
    //      main->a->c->e->malloc
    //      main->a->d->e->malloc
    //      main->b->c'->e->malloc
    //      main->b->d'->e->malloc
    ////////////////////////////////////////////////////////////////////////////////

    void doCloningClassical() {
        std::vector<std::set<std::string>> layers;

        // Roots are at context layer -1.
        int l = -1;

        // All allocating functions are roots.
        layers.emplace_back();
        for (auto & fn : allocFnMap)
            layers[l + 1].insert(fn.first);

        // Start at roots and compute the layers.
        while (true) {
            layers.emplace_back();

            // Add callers of this layer to the next layer.
            for (auto & node : layers[l + 1])
                layers[l + 2].insert(str_buCG[node].begin(),
                                     str_buCG[node].end());

            // All nodes in the layer were leaves of the bottom-up call graph.
            if (layers[l + 2].empty())
                break;

            l += 1;

            if (l == CompassDepth)
                break;
        }

        // Go through the layers and do the cloning.
        for (int k = l; k >= 1; k -= 1) {
            auto callees = layers[k]; // By-val.. we modify layers[k].

            for (auto & callee : callees) {
                // If the function we need to clone is not in this file,
                // we need to "symbolically" clone it. This just means that
                // we put its name in our representations of the call graph
                // that just deal with function names (str_CG, str_buCG), but
                // don't try to clone an actual function.

                bool sym = false; // do symbolic cloning

                Function * callee_fn = theModule->getFunction(callee);
                if (!callee_fn)
                    sym = true;

                // callers := layers[k + 1] ∩ { node | node is a caller of
                // callee }
                std::vector<std::string> callers;
                std::set_intersection(
                    layers[k + 1].begin(), layers[k + 1].end(),
                    str_buCG[callee].begin(), str_buCG[callee].end(),
                    std::back_inserter(callers));

                // There needs to be a copy of callee for each caller of callee.
                for (auto & caller : callers) {
                    // We already have one copy of the function, the original.
                    if (&caller == &(*callers.begin()))
                        continue;

                    // We need to do some actual cloning.
                    if (!sym) {
                        Function * fclone = createClone(callee_fn);
                        std::string new_name = fclone->getName().str();
                        times1Calls2[caller+new_name] = 1;

                        // Copy edges for clone from callee.
                        str_buCG[new_name].clear();
                        str_CG[new_name] = str_CG[callee];
                        for (auto & e : str_CG[new_name]) {
                            str_buCG[e].insert(new_name);
                            times1Calls2[new_name+e] = times1Calls2[callee+e];
                        }

                        // Remove edge from caller to/from callee.
                        str_CG[caller].erase(callee);
                        str_buCG[callee].erase(caller);

                        // Create edges for caller to/from clone.
                        str_CG[caller].insert(new_name);
                        str_buCG[new_name].insert(caller);

                        layers[k].insert(new_name);

                        cloneLink[new_name] = callee;

                        ncloned += 1;

                        // If the caller is not in this module, we don't need to
                        // do anything else here.
                        Function * caller_fn = theModule->getFunction(caller);
                        if (!caller_fn)
                            continue;

                        // Find sites in the caller that call the callee.
                        // Replace them with call sites to the newly created
                        // clone.
                        for (auto & BB : *caller_fn) {
                            for (auto it = BB.begin(); it != BB.end(); it++) {
                                if (isCallSite(&*it)) {
                                    CallSite site(&*it);
                                    Function * called = getCalledFunction(site);
                                    if (called == callee_fn) {
                                        CallSite site_clone(site->clone());
                                        setCalledFunction(site_clone, fclone);
                                        ReplaceInstWithInst(
                                            BB.getInstList(), it,
                                            site_clone.getInstruction());
                                    }
                                }
                            }
                        }
                    } else {
                        // We just need to symbolically clone the function.
                        std::string new_name = newCloneName(callee);
                        times1Calls2[caller+new_name] = 1;
                        // Copy edges for clone from callee.
                        str_buCG[new_name].clear();
                        str_CG[new_name] = str_CG[callee];
                        for (auto & e : str_CG[new_name]) {
                            str_buCG[e].insert(new_name);
                            times1Calls2[new_name+e] = times1Calls2[callee+e];
                        }

                        // Remove edge from caller to/from callee.
                        str_CG[caller].erase(callee);
                        str_buCG[callee].erase(caller);
                        
                        // Create edges for caller to/from clone.
                        str_CG[caller].insert(new_name);
                        str_buCG[new_name].insert(caller);

                        layers[k].insert(new_name);

                        cloneLink[new_name] = callee;
                    }

                }
            }
        }
    }

    void doCloningExtended() {
        std::vector<std::set<std::string>> layers;

        // Roots are at context layer -1.
        int l = -1;

        // All allocating functions are roots.
        layers.emplace_back();
        for (auto & fn : allocFnMap)
            layers[l + 1].insert(fn.first);

        // Start at roots and compute the layers.
        while (true) {
            layers.emplace_back();

            // Add callers of this layer to the next layer.
            for (auto & node : layers[l + 1])
                layers[l + 2].insert(str_buCG[node].begin(),
                                     str_buCG[node].end());

            // All nodes in the layer were leaves of the bottom-up call graph.
            if (layers[l + 2].empty())
                break;

            l += 1;

            if (l == CompassDepth)
                break;
        }

        // Go through the layers and do the cloning.
        for (int k = l; k >= 1; k -= 1) {
            auto callees = layers[k]; // By-val.. we modify layers[k].

            for (auto & callee : callees) {
                // If the function we need to clone is not in this file,
                // we need to "symbolically" clone it. This just means that
                // we put its name in our representations of the call graph
                // that just deal with function names (str_CG, str_buCG), but
                // don't try to clone an actual function.

                bool sym = false; // do symbolic cloning

                Function * callee_fn = theModule->getFunction(callee);
                if (!callee_fn)
                    sym = true;

                // callers := layers[k + 1] ∩ { node | node is a caller of
                // callee }
                std::vector<std::string> callers;
                std::set_intersection(
                    layers[k + 1].begin(), layers[k + 1].end(),
                    str_buCG[callee].begin(), str_buCG[callee].end(),
                    std::back_inserter(callers));

                // There needs to be a copy of callee for each caller of callee.
                for (auto & caller : callers) {
                    // Loop through each call for extended call graph.
                    for (int call_n = 0; call_n < times1Calls2[caller + callee]; call_n += 1) {
                        // We already have one copy of the function, the original.
                        if (call_n == 0 && &caller == &(*callers.begin()))
                            continue;
                    
                        // We need to do some actual cloning.
                    if (!sym) {
                        Function * fclone = createClone(callee_fn);
                        std::string new_name = fclone->getName().str();
                        times1Calls2[caller+new_name] = 1;

                        // Copy edges for clone from callee.
                        str_buCG[new_name].clear();
                        str_CG[new_name] = str_CG[callee];
                        for (auto & e : str_CG[new_name]) {
                            str_buCG[e].insert(new_name);
                            times1Calls2[new_name+e] = times1Calls2[callee+e];
                        }

                        // If there is only one edge, but we have made it this far,
                        // there is another caller taking the original edges, so we
                        // remove ours here.
                        if (times1Calls2[caller+callee] == 1) {
                            // Remove edge from caller to/from callee.

                            str_CG[caller].erase(callee);
                            str_buCG[callee].erase(caller);
                        }

                        // Create edges for caller to/from clone.
                        str_CG[caller].insert(new_name);
                        str_buCG[new_name].insert(caller);

                        layers[k].insert(new_name);

                        cloneLink[new_name] = callee;

                        ncloned += 1;

                        // If the caller is not in this module, we don't need to
                        // do anything else here.
                        Function * caller_fn = theModule->getFunction(caller);
                        if (!caller_fn)
                            continue;

                        // Find sites in the caller that call the callee.
                        // Replace them with call sites to the newly created
                        // clone.
                        for (auto & BB : *caller_fn) {
                            for (auto it = BB.begin(); it != BB.end(); it++) {
                                if (isCallSite(&*it)) {
                                    CallSite site(&*it);
                                    Function * called = getCalledFunction(site);
                                    if (called == callee_fn) {
                                        CallSite site_clone(site->clone());
                                        setCalledFunction(site_clone, fclone);
                                        ReplaceInstWithInst(
                                            BB.getInstList(), it,
                                            site_clone.getInstruction());
                                        // Jump out early so that on the next iterations, clones
                                        // for multiple calls will replace the next site.
                                        goto out;
                                    }
                                }
                            }
                        }
                    } else {
                        // We just need to symbolically clone the function.
                        std::string new_name = newCloneName(callee);
                        times1Calls2[caller+new_name] = 1;
                        // Copy edges for clone from callee.
                        str_buCG[new_name].clear();
                        str_CG[new_name] = str_CG[callee];
                        for (auto & e : str_CG[new_name]) {
                            str_buCG[e].insert(new_name);
                            times1Calls2[new_name+e] = times1Calls2[callee+e];
                        }

                        if (times1Calls2[caller+callee] == 1) {
                            // Remove edge from caller to/from callee.
                            str_CG[caller].erase(callee);
                            str_buCG[callee].erase(caller);
                        }
                        
                        // Create edges for caller to/from clone.
                        str_CG[caller].insert(new_name);
                        str_buCG[new_name].insert(caller);

                        layers[k].insert(new_name);

                        cloneLink[new_name] = callee;
                    }

                    // So sorry about this.
out:
                        (void)1;
                    }
                }
            }
        }
    }
    ////////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////// getSiteID
    ////////////////////////////////////////////////////////////////////////////////
    // Returns a unique site ID given a function and the
    // position of all enumerated sites in that function.
    // Works based on consistent ordering of nodes in sets
    // and maps (str_buCG).
    ////////////////////////////////////////////////////////////////////////////////
    unsigned int getSiteID(std::string fn, unsigned int site) {
        unsigned int id = 1;

        std::set<std::string> callers;
        for (auto & fn : allocFnMap)
            callers.insert(str_buCG[fn.first].begin(),
                           str_buCG[fn.first].end());

        for (auto & caller : callers) {
            if (caller == fn)
                break;

            id += sitesPerFn[cloneLink[caller]];
        }

        id += site;
        return id;
    }
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////// emitDebugLocation
    // Ouput the source locations and calling context for each allocation
    // instruction after transformation.
    ////////////////////////////////////////////////////////////////////////////////
    void emitDebugLocation(Instruction * inst, const std::string & alloc_fn, unsigned int id) {
        if (CompassDetail) {
            auto & loc = inst->getDebugLoc();
            if (loc) {
                std::stringstream buff;

                auto BB = inst->getParent();
                std::vector<std::string> bt;
                std::string p = BB->getParent()->getName();
                for (int i = 0; i <= CompassDepth; i += 1) {
                    bt.push_back(p);
                    if (str_buCG[p].empty())
                        break;
                    p = *str_buCG[p].begin();
                }

                buff
                    << id
                    << " "
                    << alloc_fn
                    << " "
                    ;
                if (loc.getInlinedAt())
                    buff 
                        << "(inlined) "
                        ;
                buff
                    << cast<DIScope>(loc.getScope())->getFilename().str()
                    << ":"
                    << loc.getLine()
                    << " in "
                    << theModule->getName().str()
                    << "\n"
                    ;

                std::string str_inst;
                raw_string_ostream rso(str_inst);
                inst->print(rso);

                buff
                    << rso.str()
                    << "\n"
                    ;

                for (const std::string & f : bt) {
                    buff
                        << "    ("
                        << f
                        << ")"
                        << "\n"
                        ;
                }

                std::string _buff = buff.str();
                fprintf(contexts_file, "%s\n", _buff.c_str());
            } else {
                fprintf(contexts_file, "no debug info.. did you compile with '-g'?\n");
            }
        } else {
            std::stringstream buff;

            auto BB = inst->getParent();
            std::vector<std::string> bt;
            std::string p = BB->getParent()->getName();
            for (int i = 0; i <= CompassDepth; i += 1) {
                bt.push_back(p);
                if (str_buCG[p].empty())
                    break;
                p = *str_buCG[p].begin();
            }

            buff
                << id
                << " "
                << alloc_fn
                << " "
                ;

            for (const std::string & f : bt) {
                buff
                    << "    ("
                    << f
                    << ")"
                    << "\n"
                    ;
            }
            
            std::string _buff = buff.str();
            fprintf(contexts_file, "%s\n", _buff.c_str());
        }
    }
    ////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////////// transformCallSites
    ////////////////////////////////////////////////////////////////////////////////
    // Replaces allocation sites with the corresponding ben* routines.
    // May add additional arguments indicating the unique site ID.
    ////////////////////////////////////////////////////////////////////////////////
    void transformCallSites() {
        // Cache of mappings.
        std::map<std::string, Function *> fnMap;
        // Make sure we only use on copy of declarations.
        std::map<std::string, Function *> declaredMap;

        // Convenience lambda.
        // If the new function is not in fnMap, it is created and put in the
        // map.
        auto get = [&](std::string & name,
                       std::map<std::string, std::string> & theMap) {
            // Look for the function in the mapping cache.
            auto search = fnMap.find(name);
            if (search != fnMap.end())
                return search->second;

            // Also see if we have already declared it.
            auto declsearch = declaredMap.find(theMap[name]);
            if (declsearch != declaredMap.end())
                return declsearch->second;

            Function * original_fn = theModule->getFunction(name);

            // If the function isn't in this module, it couldn't have been used
            // and therefore there are no sites to process. Should cause an
            // error.
            if (!original_fn)
                return (fnMap[name] = nullptr);

            FunctionType * fn_t = dyn_cast<FunctionType>(
                original_fn->getType()->getPointerElementType());

            if (!fn_t) {
                fprintf(stderr, "bad fn_t\n");
                exit(1);
            }

            // Create a copy of the function declaration.
            // Add an argument to the type if it is an allocation site
            // and change the name to *ben*.
            std::vector<Type *> paramTypes{};

            if (&theMap == &allocFnMap)
                paramTypes.push_back(IntegerType::get(myContext, 32));
            for (auto t : fn_t->params())
                paramTypes.push_back(t);

            FunctionType * new_t = FunctionType::get(
                original_fn->getReturnType(), paramTypes, fn_t->isVarArg());

            Function * new_fn = Function::Create(
                new_t, original_fn->getLinkage(), theMap[name], theModule);

            declaredMap[theMap[name]] = new_fn;

            return (fnMap[name] = new_fn);
        };

        // Do the actual work.
        // Find all call sites and replace.
        // Layer is the set of all functions that call
        // allocation/deallocation routines.
        std::set<std::string> layer;
        for (auto & fn : allocFnMap)
            layer.insert(str_buCG[fn.first].begin(), str_buCG[fn.first].end());
        for (auto & fn : dallocFnMap)
            layer.insert(str_buCG[fn.first].begin(), str_buCG[fn.first].end());

        // Search them for the call sites.
        for (auto & caller : layer) {
            Function * fn = theModule->getFunction(caller);

            if (!fn || fn->isDeclaration())
                continue;

            // Collect the sites.
            std::vector<CallSite> sites;
            for (auto & BB : *fn) {
                for (auto inst = BB.begin(); inst != BB.end(); inst++) {
                    if (isCallSite(&*inst)) {
                        CallSite site(&*inst);
                        Function * called = getCalledFunction(site);

                        // If called is NULL here, that indicates that the call
                        // is a non-trivial indirect call. We don't do anything
                        // about those in this pass, so we just move on.
                        // See getCalledFunction() for info about the indirect
                        // calls that we do intercept.
                        if (!called)
                            continue;

                        std::string called_name = called->getName().str();
                        if (allocFnMap.find(called_name) != allocFnMap.end() ||
                            dallocFnMap.find(called_name) != dallocFnMap.end())
                            sites.push_back(site);
                    }
                }
            }

            std::map<Instruction*, unsigned int> ids;

            // Replace and add site ID argument.
            unsigned int i = 0;
            for (CallSite & site : sites) {
                Function * fn = getCalledFunction(site);
                std::string fn_name = fn->getName().str();

                std::vector<Value *> args{};
                Instruction * newcall = nullptr;

                if (allocFnMap.find(fn_name) != allocFnMap.end()) {
                    // If it's an allocation function, we will add a site ID
                    // argument to the call.
                    unsigned int id = getSiteID(caller, i++);
                    args.push_back(
                        ConstantInt::get(IntegerType::get(myContext, 32),
                                         id));

                    for (auto it = arg_begin(site); it != arg_end(site); it++)
                        args.push_back(*it);

                    // CallInst vs InvokeInst differences.
                    if (isa<CallInst>(site.getInstruction())) {
                        newcall =
                            CallInst::Create(get(fn_name, allocFnMap), args);
                    } else {
                        InvokeInst * inv =
                            dyn_cast<InvokeInst>(site.getInstruction());
                        BasicBlock * norm = inv->getNormalDest();
                        BasicBlock * unwind = inv->getUnwindDest();
                        newcall = InvokeInst::Create(get(fn_name, allocFnMap),
                                                     norm, unwind, args);
                    }

                    ids[newcall] = id;

                    n_sites += 1;
                } else if (dallocFnMap.find(fn_name) != dallocFnMap.end()) {
                    for (auto it = arg_begin(site); it != arg_end(site); it++)
                        args.push_back(*it);

                    if (isa<CallInst>(site.getInstruction())) {
                        newcall =
                            CallInst::Create(get(fn_name, dallocFnMap), args);
                    } else {
                        InvokeInst * inv =
                            dyn_cast<InvokeInst>(site.getInstruction());
                        BasicBlock * norm = inv->getNormalDest();
                        BasicBlock * unwind = inv->getUnwindDest();
                        newcall = InvokeInst::Create(get(fn_name, dallocFnMap),
                                                     norm, unwind, args);
                    }
                }

                ReplaceInstWithInst(site.getInstruction(), newcall);

                if (allocFnMap.find(fn_name) != allocFnMap.end()) {
                    emitDebugLocation(newcall, fn_name, ids[newcall]);
                }
            }
        }
    }
    ////////////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////// runOnModule
    ////////////////////////////////////////////////////////////////////////////////
    bool runOnModule(Module & module) {
        theModule = &module;

        if (CompassMode == "analyze") {
            // Clear files from previous runs.
            contexts_file = fopen(SOURCE_LOCATIONS_FILE, "w");
            nsites_file = fopen(NSITES_FILE, "w");
            nclones_file = fopen(NCLONES_FILE, "w");
            fclose(contexts_file);
            fclose(nsites_file);
            fclose(nclones_file);

            buildBottomUpCG();

            std::ofstream os(BOTTOM_UP_CALL_GRAPH_FILE);

            writeBottomUpCG(os);

            os.close();

            //fprintf(stderr, "wrote to buCG.txt\n");

            // Don't bother doing verification or writing bitcode..
            if (CompassQuickExit)
                _exit(0);

            return false;
        } else if (CompassMode == "transform") {
            contexts_file = sync_fopen(SOURCE_LOCATIONS_FILE);
            nsites_file   = sync_fopen(NSITES_FILE);
            nclones_file   = sync_fopen(NCLONES_FILE);

            std::ifstream is(BOTTOM_UP_CALL_GRAPH_FILE);
            readBottomUpCG(is);

            for (auto & it : sitesPerFn) {
                Function * f = theModule->getFunction(it.first);
                if (f && !f->isDeclaration())
                    pre_n_sites += it.second;
            }

            flipCG();

            if (CompassClassicalCG) {
                doCloningClassical();
            } else {
                doCloningExtended();
            }

            //transformCallSites();

            //fprintf(stderr, "%u function clones created\n", ncloned);
            //fprintf(stderr, "%llu allocation sites\n", n_sites);

            fprintf(nsites_file, "%llu %llu\n", pre_n_sites, n_sites);

            fprintf(nclones_file, "%llu\n", ncloned);

#if LLVM_VERSION_MAJOR >= 4
            if (CompassQuickExit) {
                // Writing the bitcode ourselves is faster.
                //fprintf(stderr, "writing bitcode..\n", n_sites);

                // Try to stay consistent with the output behavior of opt.
                auto * option = static_cast<llvm::cl::opt<std::string> *>(
                    llvm::cl::getRegisteredOptions().lookup("o"));

                std::string oname = option->getValue();
                int fd;
                if (oname == "-")
                    fd = STDOUT_FILENO;
                else
                    fd = open(oname.c_str(), O_WRONLY | O_CREAT);

                raw_fd_ostream os(fd, true);
                WriteBitcodeToFile(theModule, os);
                os.flush();
                os.close();

                // Exit now. Skip verification.
                _exit(0);
            }
#endif
            return true;
        } else {
            fprintf(stderr, "'%s' is an invalid compass mode.\n",
                    CompassMode.c_str());
            return false;
        }

        return true;
    }
    ////////////////////////////////////////////////////////////////////////////////
};

} // namespace
char compass::ID = 0;
static RegisterPass<compass> X("compass", "compass Pass",
                               false /* Only looks at CFG */,
                               false /* Analysis Pass */);


/* This LLVM compiler pass was written by Brandon Kammerdiener
 * for the University of Tennessee, Knoxville. */

#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdio.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <vector>

using namespace llvm;

cl::opt<int>
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

static LLVMContext myContext;

namespace {

struct compass : public ModulePass {
    static char ID;

    unsigned int ncloned;
    std::map<Function *, std::set<Function *>> buCG;
    std::map<std::string, std::set<std::string>> str_buCG;
    std::map<std::string, std::set<std::string>> str_CG;
    std::set<std::string> relevantNodes;
    std::map<std::string, std::string> cloneLink;
    std::map<std::string, unsigned int> sitesPerFn;
    std::map<std::string, unsigned int> namecount;
    unsigned long long n_sites;
    Module * theModule;
    std::map<std::string, std::string> allocFnMap;
    std::map<std::string, std::string> dallocFnMap;

    compass() : ModulePass(ID), ncloned(0), n_sites(0), theModule(nullptr) {
        allocFnMap["malloc"] = "sg_alloc_exact";
        allocFnMap["_Znam"] = "sg_alloc_exact";
        allocFnMap["_Znwm"] = "sg_alloc_exact";
        /*
        allocFnMap["calloc"] = "ben_calloc";
        allocFnMap["realloc"] = "ben_realloc";
        allocFnMap["f90_alloc"] = "f90_ben_alloc";
        allocFnMap["f90_alloc03"] = "f90_ben_alloc03";
        allocFnMap["f90_alloc03_chk"] = "f90_ben_alloc03_chk";
        allocFnMap["f90_alloc04"] = "f90_ben_alloc04";
        allocFnMap["f90_alloc04_chk"] = "f90_ben_alloc04_chk";
        allocFnMap["f90_kalloc"] = "f90_ben_kalloc";
        allocFnMap["f90_calloc"] = "f90_ben_calloc";
        allocFnMap["f90_calloc03"] = "f90_ben_calloc03";
        allocFnMap["f90_calloc04"] = "f90_ben_calloc04";
        allocFnMap["f90_kcalloc"] = "f90_ben_kcalloc";
        allocFnMap["f90_ptr_alloc"] = "f90_ben_ptr_alloc";
        allocFnMap["f90_ptr_alloc03"] = "f90_ben_ptr_alloc03";
        allocFnMap["f90_ptr_alloc04"] = "f90_ben_ptr_alloc04";
        allocFnMap["f90_ptr_src_alloc03"] = "f90_ben_ptr_src_alloc03";
        allocFnMap["f90_ptr_src_alloc04"] = "f90_ben_ptr_src_alloc04";
        allocFnMap["f90_ptr_src_calloc03"] = "f90_ben_ptr_src_calloc03";
        allocFnMap["f90_ptr_src_calloc04"] = "f90_ben_ptr_src_calloc04";
        allocFnMap["f90_ptr_kalloc"] = "f90_ben_ptr_kalloc";
        allocFnMap["f90_ptr_calloc"] = "f90_ben_ptr_calloc";
        allocFnMap["f90_ptr_calloc03"] = "f90_ben_ptr_calloc03";
        allocFnMap["f90_ptr_calloc04"] = "f90_ben_ptr_calloc04";
        allocFnMap["f90_ptr_kcalloc"] = "f90_ben_ptr_kcalloc";
        allocFnMap["f90_auto_allocv"] = "f90_ben_auto_allocv";
        allocFnMap["f90_auto_alloc"] = "f90_ben_auto_alloc";
        allocFnMap["f90_auto_alloc04"] = "f90_ben_auto_alloc04";
        allocFnMap["f90_auto_calloc"] = "f90_ben_auto_calloc";
        allocFnMap["f90_auto_calloc04"] = "f90_ben_auto_calloc04";
        */

        dallocFnMap["free"] = "sg_free";
        dallocFnMap["_ZdaPv"] = "sg_free";
        dallocFnMap["_ZdlPv"] = "sg_free";
        /*
        dallocFnMap["f90_dealloc"] = "f90_ben_dealloc";
        dallocFnMap["f90_dealloc03"] = "f90_ben_dealloc03";
        dallocFnMap["f90_dealloc_mbr"] = "f90_ben_dealloc_mbr";
        dallocFnMap["f90_dealloc_mbr03"] = "f90_ben_dealloc_mbr03";
        dallocFnMap["f90_deallocx"] = "f90_ben_deallocx";
        dallocFnMap["f90_auto_dealloc"] = "f90_ben_auto_dealloc";
        */
    }

    void getAnalysisUsage(AnalysisUsage & AU) const {
        AU.addRequired<CallGraphWrapperPass>();
    }

    ///////////// Make the use of CallSite generic across CallInst and InvokeInst
    /////////////////////////////////////////////////////////////////////////////
    bool isCallSite(llvm::Instruction * inst) {
        return isa<CallInst>(inst) || isa<InvokeInst>(inst);
    }

    Function * getCalledFunction(CallSite & site) {
        CallInst * call = dyn_cast<CallInst>(site.getInstruction());
        if (call)
            return call->getCalledFunction();
        return dyn_cast<InvokeInst>(site.getInstruction())->getCalledFunction();
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
    void buildBottomUpCG(CallGraph & CG) {
        std::set<CallGraphNode *> visited;

        // recursive convenience lambda
        std::function<void(CallGraphNode *)> rec_search =
            [&](CallGraphNode * node) {
                Function * f = node->getFunction();

                if (!f)
                    return;

                if (visited.find(node) != visited.end())
                    return;
                visited.insert(node);

                buCG[f];

                for (auto & call_site : *node) {
                    CallGraphNode * cgnode = call_site.second;
                    Function * callee = cgnode->getFunction();
                    if (callee) {
                        buCG[callee].insert(f);
                        rec_search(cgnode);
                    }
                }
            };

        for (auto & node : CG) {
            rec_search(node.second.get());
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
                stream << " " << caller->getName().str();
            }
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
            for (unsigned int j = 0; j < n_callers; j += 1) {
                stream >> caller;
                str_buCG[callee].insert(caller);
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
            fclone = CloneFunction(fn, VMap);
            fclone->setName(name);
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

    void doCloning(CallGraph & CG) {
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
                        CG.getOrInsertFunction(fclone);

                        // Copy edges for clone from callee.
                        str_buCG[new_name].clear();
                        str_CG[new_name] = str_CG[callee];
                        for (auto & e : str_CG[new_name])
                            str_buCG[e].insert(new_name);

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

                        // Copy edges for clone from callee.
                        str_buCG[new_name].clear();
                        str_CG[new_name] = str_CG[callee];
                        for (auto & e : str_CG[new_name])
                            str_buCG[e].insert(new_name);

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
    ////////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////////////////////////////////////////// getSiteID
    ////////////////////////////////////////////////////////////////////////////////
    // Returns a unique site ID given a function and the
    // position of all enumerated sites in that function.
    // Works based on consistent ordering of nodes in sets
    // and maps (str_buCG).
    ////////////////////////////////////////////////////////////////////////////////
    unsigned int getSiteID(std::string fn, unsigned int site) {
        unsigned int id = 1; // We want to start at 1 to reserve 0 for indirect calls

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
                    args.push_back(
                        ConstantInt::get(IntegerType::get(myContext, 32),
                                         getSiteID(caller, i++)));

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
            }
        }
    }
    ////////////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////// runOnModule
    ////////////////////////////////////////////////////////////////////////////////
    bool runOnModule(Module & module) {
        theModule = &module;

        CallGraph & CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();

        if (CompassMode == "analyze") {
            buildBottomUpCG(CG);

            std::ofstream os("buCG.txt");

            writeBottomUpCG(os);

            os.close();

            fprintf(stderr, "wrote to buCG.txt\n");

            // Don't bother doing verification or writing bitcode..
            if (CompassQuickExit)
                _exit(0);

            return false;
        } else if (CompassMode == "transform") {
            std::ifstream is("buCG.txt");
            readBottomUpCG(is);

            flipCG();

            doCloning(CG);

            transformCallSites();

            fprintf(stderr, "%u function clones created\n", ncloned);
            fprintf(stderr, "%llu allocation sites\n", n_sites);

            if (CompassQuickExit) {
                // Writing the bitcode ourselves is faster.
                fprintf(stderr, "writing bitcode..\n");

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

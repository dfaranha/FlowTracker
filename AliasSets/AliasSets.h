#ifndef __ALIAS_SETS_H__
#define __ALIAS_SETS_H__

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/AliasSetTracker.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/ADT/ilist.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/Support/Debug.h"
#include "llvm/ADT/IntEqClasses.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/CallSite.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IntrinsicInst.h"


#include <cassert>
#include<set>
#include<map>
#include<queue>
#include <utility>

using namespace llvm;

class DisjointSets {
public:
	// Alias set
    typedef DenseMap<Value*, unsigned> NodeMap;

    // Alias set
    typedef SetVector<Value*> PointersList;

    // Alias set list
    typedef SmallVector<PointersList, 4> AliasSetList;

    // Maps values to their identifier in the IntEqClasses structure
    NodeMap valuemap;
    // DisjointSets of unsigneds, provided by LLVM
    IntEqClasses eq;

    AliasSetList asl;

    

    // Constructs structure based on a list of pointers input
    DisjointSets(const PointersList& l);

    // Add a new value to the disjoint sets structure
    bool add(Value* V);

    // Gets two values and union-find them
    // If either of them is not known, raise assertion failure
    void process(Value* V1, Value* V2);

    // Prepare alias sets to be iterated. Only call this function after ALL
    // union-finding has been done
    void finish();

    size_t size()
    {
        assert(asl.size() > 0);

        return asl.size();
    }

    typedef AliasSetList::iterator iterator;

    iterator begin()
    {
        assert(asl.size() > 0);

        return this->asl.begin();
    }
    iterator end()
    {
        return this->asl.end();
    }

};

class AliasSets: public ModulePass {
private:
		int disjointSetIndex; // Handles the number of the next free index vailable for new disjoint sets
		AliasAnalysis *AA; // Handles a pointer to the AliasAnalysis pass

		llvm::DenseMap<int, int> disjointSet; // maps integers to the disjoint sets that contains the integer
		llvm::DenseMap<int, std::set<int> > disjointSets; // table of disjoint sets

		llvm::DenseMap<Value*, int> valueDisjointSet; // maps integers to the disjoint sets that contains the integer
		llvm::DenseMap<int, std::set<Value*> > valueDisjointSets; // table of disjoint sets
		DisjointSets *sets;
		int nextMemoryBlock;
		std::map<Value*, int> value2int;
        std::map<int, Value*> int2value;
        std::map<int, std::string> nameMap;


		void addValueDisjointSets(Value *v);
		bool runOnModule(Module &M);
		void printSets();

	public:
		static char ID;
		AliasSets() :
				ModulePass(ID) {
				nextMemoryBlock = 1;					
		}
		;

		void getAnalysisUsage(AnalysisUsage &AU) const;
		llvm::DenseMap<int, std::set<Value*> > getValueSets();
		llvm::DenseMap<int, std::set<int> > getMemSets();
		int getValueSetKey(Value* v);
		int getMapSetKey(int m);

		int getNewMemoryBlock() {
        	return nextMemoryBlock++;
        }
		
		int Value2Int(Value *v){
        int n;

        if (value2int.count(v))
                return value2int[v];

        n = getNewMemoryBlock();
        value2int[v] = n;
        int2value[n] = v;
//      errs() << "int " << n << "; value " << v << "\n";

        // Also get a name for it
        if (v->hasName()) {
                nameMap[n] = v->getName();
        }
        else if (isa<Constant>(v)) {
                nameMap[n] = "constant";
                //nameMap[n] += intToStr(n);
        }
        else {
                nameMap[n] = "unknown";
        }

        return n;
}


	};


#endif


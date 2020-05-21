#define DEBUG_TYPE "AliasSets"
#include "AliasSets.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

cl::opt<bool> mustAA("mustAlias", cl::desc("Only merge nodes that must alias"));

/*
 * LLVM's AliasAnalysis passes generate a solution that allows to compare pairs of hands.
 * The "alias (P1, P2)" function exported by AliasAnalysis checks whether two pointers P1 and P2 can point
 * to the same memory area. Using this function, we can build the disjoint sets of pointers used by DepGraph.
 * Each P pointer found is compared to all other pointers already analyzed. If P is an alias of some pointer
 * already analyzed, it must be included in the same disjoint set that the pointer of which it is an alias.
 * On the other hand, if P is not an alias of any pointer already examined, we create a new disjoint set for it.
 *
 * Authors: Mateus Tymburibá (mateustymbu@gmail.com) and Junio Cezar(juniocezar.dcc@gmail.com)
 * Date: May 30th, 2015
 */


DisjointSets::DisjointSets(const DisjointSets::PointersList& l) :
        eq(l.size())
{
    // Insert pointers into NodeMap
    unsigned i = 0;

    for (PointersList::const_iterator lit = l.begin(), lend = l.end();
            lit != lend; ++lit) {
        Value* V = *lit;
        valuemap[V] = i++;
//      valuemap.FindAndConstruct(V).second = i++;
    }
}

bool DisjointSets::add(Value* V)
{
    if (valuemap.count(V))
        return false;

    unsigned n = valuemap.size();

    eq.grow(n + 1);
    valuemap[V] = n + 1;

    return true;
}

void DisjointSets::process(Value* V1, Value* V2)
{
    NodeMap::const_iterator mit1 = valuemap.find(V1);
    NodeMap::const_iterator mit2 = valuemap.find(V2);

    assert(mit1 != valuemap.end() and mit2 != valuemap.end());

    unsigned id1 = mit1->second;
    unsigned id2 = mit2->second;

    eq.join(id1, id2);
}

void DisjointSets::finish()
{
    eq.compress();

    unsigned num_classes = eq.getNumClasses();

    asl.resize(num_classes);

    for (NodeMap::const_iterator mit = valuemap.begin(), mend =
            valuemap.end(); mit != mend; ++mit) {

        Value* V = mit->first;
        unsigned id = mit->second;

        unsigned eq_class = eq[id];

        asl[eq_class].insert(V);
    }
}

bool AliasSets::runOnModule(Module &M) {

    // Calls LLVM AliasAnalysis pass to check pointers
    AA = &getAnalysis<AliasAnalysis>();
    SetVector<Value*> initial_list_pointers;
    disjointSetIndex = 0;

    
    std::map<int, std::set<int> > allPointsTo;
    for (Module::iterator F_ = M.begin(), Mend = M.end(); F_ != Mend; ++F_) {

    Function &F = *F_;
        
    /* The Interprocedural matching was manly implemented by 
     *  Victor Hugo Sperle Campos for his AliasSet pass 
     */
    
    // Interprocedural matching

    // Store all return values of this function
    SetVector<Value*> return_values;

    for (Function::iterator Fit = F.begin(), Fend = F.end(); Fit != Fend;
        ++Fit) {
        BasicBlock &BB = *Fit;
        ReturnInst *ret = dyn_cast<ReturnInst>(BB.getTerminator());

        if (ret) {
            Value *return_val = ret->getReturnValue();
            if (return_val) {
                return_values.insert(return_val);
            }
        }
    }

    // Find all call sites of this function to perform matching
    // This follows the logic of IPConstantProp of LLVM
    for (Value::user_iterator UI = F.user_begin(), E = F.user_end(); UI != E;
                ++UI) {
        User *U = *UI;
        
        // Ignore blockaddress uses.
        if (isa<llvm::BlockAddress>(U))
            continue;

        // Ignore MemCopyInstructions
        if ( isa<MemCpyInst>(U) )
            continue;

        // Used by a non-instruction, or not the callee of a function, do not
        // transform.
        if (!isa<CallInst>(U) && !isa<InvokeInst>(U))
            continue;

        CallSite CS(cast<Instruction>(U));
        if (!CS.isCallee(&UI.getUse()))
            continue;

        

        Instruction *call = CS.getInstruction();

        CallSite::arg_iterator AI = CS.arg_begin();
        CallSite::arg_iterator AIend = CS.arg_end();
        Function::arg_iterator Arg = F.arg_begin();
        //Function::arg_iterator Argend = F.arg_end();

        // Match arguments
        for (; AI != AIend; ++AI, ++Arg) {
        
            Value *real_arg = AI->get();
            Value *formal_arg = Arg;            

            if (formal_arg->getType()->isPointerTy()) {                    
                int idx_1 = Value2Int(real_arg);
                int idx_2 = Value2Int(formal_arg);
               // errs() <<"\t\t" << real_arg->getName() << " <-> " << formal_arg->getName() << " (" << F.getName() << ")\n";
                allPointsTo[idx_2].insert(idx_1);
                }
            }

            // Match return values
            for (SetVector<Value*>::iterator sit = return_values.begin(), send =
                    return_values.end(); sit != send; ++sit) {
                int idx_1 = Value2Int(call);
                int idx_2 = Value2Int(*sit);
                allPointsTo[idx_1].insert(idx_2);
            }
        }

        /* Victor's implementation goes until here */


        // Looks for function arguments
        for (Function::arg_iterator ait = F.arg_begin(), aend = F.arg_end(); ait != aend; ++ait) {
            // If the instruction argument is a pointer
            if (ait->getType()->isPointerTy()) {
                // Puts the pointer in a list of pointers
                 initial_list_pointers.insert(ait);
            }
        }

        // Looks for instructions (a instruction is the same as its result/definition)
        for (Function::iterator BB = F.begin(), Fend = F.end(); BB != Fend; ++BB) {
            for (BasicBlock::iterator I = BB->begin(), BBend = BB->end(); I != BBend; ++I) {
                // If the instruction result will be writen in a pointer
                if (I->getType()->isPointerTy()) {
                    // Puts the pointer in a set of pointers
                    initial_list_pointers.insert(I);
                }
            }
        }
    }

    
    // Printing the allPointsTo set (Debug info)
    //for (std::map<int, std::set<int> >::iterator i = allPointsTo.begin(), e =
           // allPointsTo.end(); i != e; ++i) {
   // errs() << i->first << "-> {" ;
      //  for (std::set<int>::iterator ii = i->second.begin(), ee = i->second.end(); ii != ee; ++ii) {
          //  errs() << *ii << " ";
      //  }
      //  errs() << "}\n";
  //  }
   // errs() <<"\n\n--\n";

    size_t n = initial_list_pointers.size();

    //iterate over every pair of pointers in order to find sets
    for (unsigned i = 0; i < n; ++i) {      
        Value *val_1 = initial_list_pointers[i];
        // associate a number with the primary pointer
        int idx_1 = Value2Int(val_1);
        // get the Function the contains the pointer ** we only compare 
        // pointer within the same function **
        Instruction *I_ = dyn_cast<Instruction>(val_1);
        Function *f1 = I_ == NULL ? NULL : I_->getParent()->getParent();
        for (unsigned j = i + 1; j < n; ++j) {
            Value* val_2 = initial_list_pointers[j];
            int idx_2 = Value2Int(val_2);
            Instruction *I2_ = dyn_cast<Instruction>(val_2);
            Function *f2 = I2_ == NULL ? NULL : I2_->getParent()->getParent();

            // if the pointers are valid within a same function, we compare them
            if((f1 != NULL && f2 != NULL) && (f1 == f2)){
                switch (this->AA->alias(val_1,val_2)) {             
                    case PartialAlias:
                        if (! allPointsTo[idx_1].count(idx_2) && !mustAA)
                            allPointsTo[idx_1].insert(idx_2);                                       
                        break;
                    case MayAlias:
                        if (! allPointsTo[idx_1].count(idx_2)  && !mustAA)
                            allPointsTo[idx_1].insert(idx_2);                                       
                        break;
                    case MustAlias:
                    // Creating sets allPointsTo that are not disjoint.
                        if (! allPointsTo[idx_1].count(idx_2) )
                            allPointsTo[idx_1].insert(idx_2);                                       
                        break;
                
                    case NoAlias:
                        break;
                }   
            }
        }
    }



    // Printing the allPointsTo set (Debug info)
    // for (std::map<int, std::set<int> >::iterator i = allPointsTo.begin(), e =
    //        allPointsTo.end(); i != e; ++i) {
   //  errs() << i->first << "-> {" ;
   //      for (std::set<int>::iterator ii = i->second.begin(), ee = i->second.end(); ii != ee; ++ii) {
   //         errs() << *ii << " ";
    //     }
    //     errs() << "}\n";
    // }

    int count = 0;

    for (std::map<int, std::set<int> >::iterator i = allPointsTo.begin(), e =
            allPointsTo.end(); i != e; ++i) {

        bool found = false;
        int disjointSetIndex;

        //First, we discover the disjoint set that contains at least one of those values.
        //if none of the values are contained in any existing set, we create a new one

                // (by mateustymbu)
                // i->first: is the pointer
                // i->second: is the set of positions pointed by the pointer
                // count: Return 1 if the specified key is in the map, 0 otherwise.

        if (!i->second.count(i->first)) { // Why is this test necessary? (by mateustymbu)

                        // (by mateustymbu) checks if the pointer is already in some disjointSet
            if (disjointSet.count(i->first)) {
                                // (by mateustymbu) take note of the disjointSet that contains the pointer
                disjointSetIndex = disjointSet[i->first];
                found = true;
            }

        }

        // (by mateustymbu) when the pointer isn't in any disjointSet yet
        if (!found){
            // (by mateustymbu) iterates over the positions pointed by the pointer
            for (std::set<int>::iterator ii = i->second.begin(), ee = i->second.end(); ii != ee; ++ii) {
                // (by mateustymbu) checks if the position pointed by the pointer is already in some disjointSet
                if (disjointSet.count(*ii)) {
                    // (by mateustymbu) take note of the disjointSet that contains the position pointed by the pointer
                    disjointSetIndex = disjointSet[*ii];
                    found = true;
                    break;
                }
            }

            // Here we assign a new disjoint set index
            // (by mateustymbu) creates a new disjointSet if neither the pointer nor its pointed positions are in any disjointSet
            if (!found) disjointSetIndex = ++count;
        }

        //Here we effectively create a new disjoint set
        if (!disjointSets.count(disjointSetIndex))  disjointSets[disjointSetIndex];


        // Gets the disjointSet that must be updated
        std::set<int> &selectedDisjointSet = disjointSets[disjointSetIndex];


        //Finally, we insert the values in the selected disjoint set
        if (!i->second.count(i->first)) {// Why is this test necessary? (by mateustymbu)
            // Inserts the pointer (by mateustymbu)
            if (!selectedDisjointSet.count(i->first)) {
                selectedDisjointSet.insert(i->first);
                disjointSet[i->first] = disjointSetIndex;
            }

        }

        for (std::set<int>::iterator ii = i->second.begin(), ee = i->second.end(); ii != ee; ++ii) {
            // Inserts the pointed positions (by mateustymbu)
            if (!selectedDisjointSet.count(*ii)) {
                selectedDisjointSet.insert(*ii);
                disjointSet[*ii] = disjointSetIndex;
            }

        }

    }


    //Now, we translate the disjoint sets to Value* disjoint sets

    //First we translate the keys
    for (llvm::DenseMap<int, int >::iterator i = disjointSet.begin(), e = disjointSet.end(); i != e; ++i) {

        if (int2value.count(i->first)) {
            valueDisjointSet[int2value[i->first]] = i->second;
        }

    }

    //Finally we translate the disjoint sets
    for (llvm::DenseMap<int, std::set<int> >::iterator i = disjointSets.begin(), e = disjointSets.end(); i != e; ++i) {
        std::set<Value*> translatedValues;
        for (std::set<int>::iterator ii = i->second.begin(), ee = i->second.end(); ii != ee; ++ii) {
            if (int2value.count(*ii)) {
                translatedValues.insert(int2value[*ii]);
            }
        }
        valueDisjointSets[i->first] = translatedValues;
    }

 

    //printSets();

    return false;
}




void AliasSets::printSets() {

    unsigned long int count = 0;

    errs() << "------------------------------------------------------------\n"
           << "                         Alias sets:                        \n"
           << "------------------------------------------------------------\n\n";

    for (llvm::DenseMap<int, std::set<Value*> >::iterator i = valueDisjointSets.begin(), e = valueDisjointSets.end(); i != e; ++i) {

        count++;
        errs() << "Set " << i->first << ": ";
        std::set<Value*>::iterator ii = i->second.begin();
        Value *val = dyn_cast<Value>(*ii);
        errs() << val->getName();
        ii++;

        for (std::set<Value*>::iterator ee = i->second.end(); ii != ee; ++ii) {
            val = dyn_cast<Value>(*ii);
            errs() << ", " << val->getName();
        }

        errs() << "\n";
    }

    errs() << "\nTotal number of alias sets (memory nodes): " << count << "\n\n";
}

llvm::DenseMap<int, std::set<llvm::Value*> > AliasSets::getValueSets() {
    return valueDisjointSets;
}

llvm::DenseMap<int, std::set<int> > AliasSets::getMemSets() {
    return disjointSets;
}

int AliasSets::getValueSetKey(Value* v) {

    if (valueDisjointSet.count(v)) return valueDisjointSet[v];

    //        assert(0 && "Value requested is not in any alias set!");
    return 0;

}

int AliasSets::getMapSetKey(int m) {

    if (disjointSet.count(m)) return disjointSet[m];
    //        assert(0 && "Memory positions requested is not in any alias set!");
    return 0;

}

void AliasSets::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AliasAnalysis>();
    AU.setPreservesAll();
}

char AliasSets::ID = 0;
static RegisterPass<AliasSets> X("alias-sets",
        "Get alias sets from alias analysis pass", false, false);


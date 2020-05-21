//===-- bSSA2.h - bSSA2 class definition -------*- C++ -*-===//
//
//                     bSSA2 - Bruno Static Single Assignment
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the bSSA2 class which is the core class
/// to Flow Tracker tools. It makes a program intermediate representation which is
/// a dependence graph with data and control edge. It
/// contains also the declaration of the Pred class which is an auxiliar
/// class that stores predicates values used to compute the control edges.
///
/// Computer Science Department - Federal Universty of Minas Gerais State - Brazil
/// Contact: Bruno Rodrigues Silva - brunors172@gmail.com
//===----------------------------------------------------------------------===//

#ifndef LLVM_BSSA2_H
#define LLVM_BSSA2_H


namespace llvm {

//STATISTIC(numSources, "Number of secret information sources");
//STATISTIC(numSink, "Number of branch instructions");
//STATISTIC(numControlEdges, "Number control edges in dependence graph");
//STATISTIC(numDataEdges, "Number data edges in dependence graph");
//STATISTIC(numInstructions, "Number of instructions");

// Class for store one predicate
class Pred {

public:
	/// \brief Make a control dependence with an instruction.
	///
	/// \param i Instruction to be stored.
	void addInst(Instruction *i);

	/// \brief Return the stored predicate.
	///
	/// \returns A value related to the predicate.
	Value *getPred() const;

	/// \brief Return the dependent instruction indexed by param i.
	///
	/// \param i Index.
	/// \returns The instruction.
	Instruction *getInst(int i) const;

	/// \brief Check if the Function content is dependent
	///
	/// \param The function.
	/// \returns True if yes, false if not.
	bool isFDependent(Function  *f) const;

	/// \brief Make a control dependent with a function.
	///
	/// \param The function.
	void addFunc(Function *f);

	/// \brief Return the total of dependent instructions.
	///
	/// \returns The total of dependent instructions.
	int getNumInstrucoes() const;

	/// \brief Constructor of the Pred class.
	///
	/// \param p The predicate.
	Pred(Value *p);

	/// \brief Return the dependent function indexed by param i.
	///
	/// \param i The index.
	/// \returns The function.
	Function *getFunc(int i) const;

	/// \brief Return the total of dependent functions.
	///
	/// \returns The total of dependent functions.
	int getNumFunctions() const;

	/// Vector for store dependent instructions
	std::vector<Instruction *> insts;

	/// \brief Check if the instruction is dependent.
	///
	/// \param i Instruction.
	/// \returns True if yes, false if not.
	bool isDependent(Instruction *i) const;

private:

	// Store the predicate
	Value *predicate;

	// Store the dependent functions
	std::vector<Function *> funcs;

};



// Class to identify control dependence and to create control edges in Dependence Graph
// This class uses several LLVM passes, including Raphael's Dependence Graph. However,
// I have coded some functions in Dependence Graph.
//
// There are other passes used here: Post Dominator Tree, Dominator Tree, Loop Info and
// Scalar Evolution passes.
//
class bSSA2: public ModulePass {

public:

	bool runOnModule(Module&);

	//Method constructor
	bSSA2() : ModulePass(ID) {
		//numInstructions=0;
	}

	//Pass identification
	static char ID;

private:

	// Variable to store FileName and Line number related to vulnerable paths in dependence graph.
	std::vector<StringRef> srcFileName, dstFileName;
	std::vector<unsigned> srcLine, dstLine;

	//src stores the instructions which are source of secret information.
	//dst stores the instructions  which are sinks.
	std::vector<Value *> src, dst;

	~bSSA2 ();

	void getAnalysisUsage(AnalysisUsage &AU) const;

	/// \brief Increase Rafael's Dependence Graph including control edges.
	///
	/// \param g The dependence graph from DepGraph LLVM Pass.
	void incGraph(Graph *g);

	/// Vector of predicates objects.
	std::vector<Pred *> predicatesVector;

	/// \brief All instructions of function F are dependent from predicate p.
	///
	/// \param F The function.
	/// \param p The predicate.
	void dependentFunction(Function *F, Pred *p);

	/// \brief Implement the optimization on gating instruction. We make a pre-order in Dominator Tree in order to get transitivity.
	///
	/// \param node Initial Basic Block.
	/// \param DT Dominator Tree.
	/// \param PD Post Dominator Tree.
	void processNode(BasicBlock *node, DominatorTree &DT, PostDominatorTree &PD, Function &F);

	/// \brief Make control dependence the instructions in basic block children.
	///
	/// \param father Basic Block Father.
	/// \param children Basic Block children.
	/// \param p  predicate that terminates basic block father.
	/// \param PD Post Dominator Tree.
	void dependentChildren(BasicBlock *father, BasicBlock *children, Pred *p,
			PostDominatorTree &PD);

	/// \brief If the instruction is control dependent return it, else return null.
	///
	/// \param i The instruction.
	/// \returns A predicate
	Pred * getPredFromInst(Instruction *i) const;


	/// \brief If a successor node is not dominated, make control dependence the PHI instruction.
	///
	/// \param node The BasicBlock.
	/// \param pred The predicate in top of stack.
	/// \param DT the Dominator Tree.
	void dependentChildrenNotDominated(BasicBlock *node, Pred *pred,
			DominatorTree &DT, PostDominatorTree &PD);
	/// \brief Add a condition, line number and file name in pool
	///
	/// \param condition A branch condition
	/// \param line The line number
	/// \param file The file name
	void addPool (Value *condition, unsigned line, StringRef file);

};

} //end namespace llvm

#endif

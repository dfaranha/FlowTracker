//===-- bSSA2.cpp - bSSA2 class  --===//
//
//                     bSSA2 - Bruno Static Single Assignment
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the method implementation of the bSSA2 class which is
/// the core class to Flow Tracker tools. It makes a program intermediate
/// representation which is a dependence graph with data and control edge. It
/// contains also the method implementation of the Pred class which is an auxiliar
/// class that stores predicates values used to compute the control edges.
///
/// Computer Science Department - Federal Universty of Minas Gerais State - Brazil
/// Contact: Bruno Rodrigues Silva - brunors172@gmail.com
//===----------------------------------------------------------------------===//



#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "../DepGraph/DepGraph.h"
#include "parserXML.h"
#include "bSSA2.h"
#include <string>

using namespace llvm;
static cl::opt<string> InputFilename("xmlfile", cl::Required,
		cl::desc("Informe um arquivo XML valido."), cl::value_desc("fileXML"));


Pred::Pred(Value *p) {
	predicate = p;

}

void Pred::addInst(Instruction *i) {
	insts.push_back(i);
}


void Pred::addFunc(Function *f) {
	funcs.push_back(f);
}


int Pred::getNumInstrucoes() const {
	return (insts.size());
}

int Pred::getNumFunctions() const{
	return (funcs.size());
}


Value *Pred::getPred() const{
	return (predicate);
}


Instruction *Pred::getInst(int i) const {
	if (i < (signed int) insts.size())
		return (insts[i]);
	else
		return NULL;
}

Function *Pred::getFunc(int i) const {
	if (i < (signed int) funcs.size())
		return (funcs[i]);
	else
		return NULL;
}


bool Pred::isDependent(Instruction *op) const {
	return std::find(insts.begin(), insts.end(), op) != insts.end();
}

bool Pred::isFDependent(Function *f) const {
	return std::find(funcs.begin(), funcs.end(), f) != funcs.end();
}


void bSSA2::getAnalysisUsage(AnalysisUsage &AU) const {
	AU.addRequired<PostDominatorTree>();
	AU.addRequired<DominatorTreeWrapperPass>();
	AU.addRequired<ModuleDepGraph>();
	AU.addRequired<LoopInfoWrapperPass>();
	AU.addRequired<ScalarEvolution>();

	// This pass will not modifies the program nor CFG
	AU.setPreservesAll();

}



Pred * bSSA2::getPredFromInst(Instruction *inst) const {
	for (unsigned int i = 0; i < predicatesVector.size(); ++i) {
		for (unsigned int j = 0; j < predicatesVector[i]->insts.size(); ++j) {
			if (predicatesVector[i]->insts[j] == inst)
				return (predicatesVector[i]);
		}
	}
	return NULL;
}



bool bSSA2::runOnModule(Module &M) {
	unsigned int Line = -1;
	// Load  XML parser which is used to read input data related to sensitive information informed by user.
	parsersXML::parserXML *xmlparser = new parsersXML::parserXML();
	if (!xmlparser->setFile(InputFilename)) {
		errs() << "Error on loading XML file." << "\n";
		return false;
	}
	std::vector<parsersXML::parserXML::funcaoFonte> srcFunctions;
	srcFunctions = xmlparser->getFontes();

	// Creating Dependence Graph (DepGraph) from source code with data edges only.
	// More Detail about DepGraph, please read ../DepGraph/DepGraph.h
	ModuleDepGraph& DepGraph = getAnalysis<ModuleDepGraph>();
	Graph *g = DepGraph.depGraph;


	// Iterates over all basic blocks getting predicates and creating relation with variable definitions.
	Function *F;
	for (Module::iterator Mit = M.begin(), Mend = M.end(); Mit != Mend; ++Mit) {
		F = Mit;
			// Iterate over all Basic Blocks of the Function
			if (F->begin() != F->end()) {
				DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>(*F).getDomTree();
				PostDominatorTree &PD = getAnalysis<PostDominatorTree>(*F);

				for (Function::iterator Fit = F->begin(), Fend = F->end();
						Fit != Fend; ++Fit) {
					processNode(Fit, DT, PD, *F);
					//Counting Instructions
					for (BasicBlock::iterator Bit = Fit->begin(), Bend =
							Fit->end(); Bit != Bend; ++Bit) {
						//++numInstructions;
					}
				}

			}
	}



	incGraph(g);

	//numControlEdges = 0;
	//numDataEdges = 0;
	//numControlEdges = g->getNumControlEdges();
	//numDataEdges = g->getNumDataEdges();

	BranchInst *bi = NULL;
	Instruction *A;
	StringRef File;
	MDNode *N;



	// For each function listed in XML file which, this part of the code find the respective function in DepGraph.
	// If success, the code get the source file name and line number of the parameter which is pointed
	// in XML input as a sensitive information.
	for (Module::iterator F = M.begin(), eM = M.end(); F != eM; ++F) {
		if (F->begin() != F->end()) {

			StringRef Name = F->getName();

			for (std::vector<int>::size_type i = 0; i != srcFunctions.size();
					++i) {
				std::string cs = srcFunctions[i].getName();
				std::size_t pos = -1;
				pos = Name.str().compare(cs);
				//pos = Name.str().find(cs);

				//if (pos != string::npos) {
				if (pos == 0) {

					for (BasicBlock::iterator inst = F->begin()->begin(), ee =
							F->begin()->end(); inst != ee; ++inst) {
						A = cast<Instruction>(inst);
						if ((N = A->getDebugLoc())) {
							DILocation *Loc = A->getDebugLoc();
							File = Loc->getFilename();
							Line = Loc->getLine();
							break;
						} else {
							File = "Unknown";
							Line = -1;
						}
					}


					for (Function::arg_iterator arg = F->arg_begin(), arg_end = F->arg_end(); arg != arg_end; ++arg) {
						bool found = false;
						for (unsigned int j = 1; !found && j < srcFunctions[i].parameters.size(); ++j) {
							if (arg->getName().str().compare(srcFunctions[i].parameters[j]) == 0)
								found = true;
						}
						for (unsigned int j = 0; !found && j < srcFunctions[i].parameters_public.size(); ++j) {
                                                        if (arg->getName().str().compare(srcFunctions[i].parameters_public[j]) == 0)
                                                                found = true;
                                                }
						if (!found) {
							errs() << "Parameter not declared: " << arg->getName().str() << "\n";
						}
					}

					for (unsigned int j = 1; j < srcFunctions[i].parameters.size(); ++j) {
						int count = 1;
						for (Function::arg_iterator arg = F->arg_begin(), arg_end = F->arg_end(); arg != arg_end; ++arg) {
							if (arg->getName().str().compare(srcFunctions[i].parameters[j]) == 0) {
								src.push_back(arg);
								srcLine.push_back(Line);
								srcFileName.push_back(File);
							}
							++count;
						}

						//If XML input informs that return value is a sensitive information, and must be tracked.
						if (srcFunctions[i].parameters[0].compare("true") == 0 || srcFunctions[i].parameters[0].compare("True") == 0) {
							for (Function::iterator Fite = F->begin(), Fend =
									F->end(); Fite != Fend; ++Fite) {
								BasicBlock &BBI = *Fite;
								ReturnInst *retI = dyn_cast<ReturnInst>(
										BBI.getTerminator());

								if (retI) {
									Value *return_val = retI->getReturnValue();
									if (return_val) {

										A = cast<Instruction>(retI);
										if ((N = A->getDebugLoc())) {
											DILocation *Loc = A->getDebugLoc();
											File = Loc->getFilename();
											Line = Loc->getLine();
										} else {
											File = "Unknown";
											Line = -1;
										}
										src.push_back(return_val);
										srcLine.push_back(Line);
										srcFileName.push_back(File);

									}
								}
							}

						}
					}

				}
			}


			// Using ScalarEvolution llvm pass in order to avoid track information flow from sensitive
			// information to branch instructions of constant loops.
			ScalarEvolution &SE = getAnalysis<ScalarEvolution>(*F);
			LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();



			// Getting source file name and line number of each branch instruction which is not a control flow of a constant loop.
			for (Function::iterator BB = F->begin(), e = F->end(); BB != e;
					++BB) {
				for (BasicBlock::iterator I = BB->begin(), ee = BB->end();
						I != ee; ++I) {


					A = cast<Instruction>(I);
					if ((N = A->getDebugLoc())) {
						DILocation *Loc = A->getDebugLoc();
						File = Loc->getFilename();
						Line = Loc->getLine();
					} else {
						File = "Unknown";
						Line = -1;
					}


					if ((bi = dyn_cast<BranchInst>(I)) && bi->isConditional()) {

						Value *condition = bi->getCondition();

						Loop *myLoop = LI.getLoopFor(bi->getParent());

						if (myLoop != NULL) {

							//If the branch instruction controls a loop
							if ((myLoop->contains(bi->getSuccessor(0))	&& !myLoop->contains(bi->getSuccessor(1)))	|| (!myLoop->contains(bi->getSuccessor(0))	&& myLoop->contains(bi->getSuccessor(1)))) {

								bool isConstantLoop = SE.hasLoopInvariantBackedgeTakenCount(myLoop)	&& isa<SCEVConstant>(SE.getBackedgeTakenCount(myLoop));

								if (!isConstantLoop) { //Insert it in the pool.
									addPool (condition, Line, File);
								}

							} else { //Does not control. Therefore insert it in the pool.
								addPool (condition, Line, File);
							}

						} else { //branch from if/else. Therefore insert it in the pool.
							addPool (condition, Line, File);
						}

				    // We are also tracking information from sensitive information pointed in XML file to flow to memory access
					} else if (I->getOpcode() == Instruction::GetElementPtr) {
						for (unsigned int f = 1; f < I->getNumOperands(); ++f) {
							addPool(I->getOperand(f), Line, File);
						}
					}


				}
			}

		}
	}




	// This part of the code, try to find one or more special paths in dependence graph created by code above (with data and control edges).
	// The special path is that connects a source of sensitive information (pointed by the user in XML input file) to a branch instruction or
	// index of memory. For each special path found, we create a 2 .dot and 1 .txt file explained below:
	//
	// 1 - subgraphLines.dot => shows the special path where the label of the nodes are file name and line numbers.
	// This is useful to find the vulnerability in C++ source file.
	//
	// 2 - subgraphASCIILines.txt => show the sequence of file name and line numbers related to special path. This is also
	// useful to find the vulnerability in C++ source file. However it is a ASCII format.
	//
	// 3 - subgraphLLVM.dot => shows the same information provided by subgraphLines.dot. However, the nodes are labeled with
	// the respective opcode, or variable name. This is more useful to LLVM developers which understand LLVM bytecodes.


	//Stats about sources and sinks
	//numSources = src.size();
	//numSink = dst.size();



	//Stats about sources and sinks
	errs() << "********* Flow Tracking Summary ************";
	errs() << "\n\nSecrets " << src.size() << "\n";
	errs() << "Branch instructions  or memory indexes " << dst.size() << "\n";

	unsigned int countWarning = 0;

	//Search paths connecting each source to each sink. If there is a path, include the path (tainted subgraph) into set of tainted subgraphs.
	int c = 0;
	//int stop=0;
	for (unsigned int i = 0; i < src.size(); ++i) {
		for (unsigned int j = 0; j < dst.size(); ++j) {

			Graph subG = g->generateSubGraph(src[i], dst[j]);

			Graph::iterator gIt = subG.begin();
			Graph::iterator gIte = subG.end();
			if (gIt != gIte) {
				++countWarning;
				//if (!stop) {
					ostringstream ss, ss0, ss1, ss0Asc, ss1Asc;
					ss0 << "./subgraphLines" << c << ".dot";
					ss0Asc << "./subgraphASCIILines" << c << ".txt";
					ss << "./subgraphLLVM" << c << ".dot";
					++c;
					ss1 << "file " << srcFileName[i].data() << " line "
							<< srcLine[i] << " to file " << dstFileName[j].data()
							<< " line " << dstLine[j] << " ";

					//ss1Asc << "file " << srcFileName[i].data() << " line "
					//		<< srcLine[i] << " to file " << dstFileName[j].data()
					//		<< " line " << dstLine[j] << " ";


					ss1Asc << " line " << srcLine[i] << " to line " << dstLine[j] << "\n	 ";

					subG.toDot(ss1.str(), ss.str()); //Make one file .dot for each tainted subgraph.
					subG.toDotLinesPruned(ss1.str(), ss0.str());
					subG.toASCIILinesPruned(dst[j], ss1Asc.str(), ss0Asc.str());
					//stop=1;
				//}
			}

		}
	}

	//errs() << "Vulnerable Subgraphs: " << countWarning << " \n(Due to Serve's memory limitation, we are showing here just one vulnerable subgraph. You can use our offline version to get all them!)\n";
	errs() << "Vulnerable Subgraphs: " << countWarning << " \n";
	//Make the file .dot including the full dependence graph.
	std::string Filename = "./fullGraph.dot";

	//Print dependency graph (in dot format).
	g->toDot("Grafo", Filename);

	return false;

}



void bSSA2::addPool (Value *condition, unsigned line, StringRef file) {
	dst.push_back(condition);
	dstLine.push_back(line);
	dstFileName.push_back(file);
}

void bSSA2::dependentChildrenNotDominated(BasicBlock *node, Pred *pred,
		DominatorTree &DT, PostDominatorTree &PD) {
	TerminatorInst *ti = node->getTerminator();
	for (unsigned int i = 0; i < ti->getNumSuccessors(); ++i) {
		if (!DT.dominates(node, ti->getSuccessor(i)) && !PD.dominates (ti->getSuccessor(i), node)) {
			for (BasicBlock::iterator bBIt = ti->getSuccessor(i)->begin(),
					bBEnd = ti->getSuccessor(i)->end(); bBIt != bBEnd; ++bBIt) {
				if (dyn_cast<Instruction>(bBIt)->getOpcode()
						== Instruction::PHI) {
					pred->addInst(bBIt);
				}
				else break;
			}
		}
	}
}



void bSSA2::processNode(BasicBlock * node, DominatorTree &DT,
		PostDominatorTree &PD, Function &F) {

	BasicBlock *BB = node;
	Value *condition;
	TerminatorInst *ti = BB->getTerminator();
	BranchInst *bi = NULL;
	SwitchInst *si = NULL;
	Pred *predicate;
	DomTreeNode *nodeTemp = DT.getNode(node);
	Pred *pred;

	if ((bi = dyn_cast<BranchInst>(ti)) && bi->isConditional()) { //If the terminator instruction is a conditional branch.

		condition = bi->getCondition();
		//Including the predicate on the predicatesVector.
		predicatesVector.push_back(new Pred(condition));
		pred = predicatesVector.back();

		//Check if some child in CFG is not dominated by node.
		dependentChildrenNotDominated(node, pred, DT, PD);

		//Make control dependence to children in the dominance tree.
		for (unsigned int i = 0; i < nodeTemp->getNumChildren(); ++i) {
			dependentChildren(node, nodeTemp->getChildren()[i]->getBlock(), pred, PD);
		}
	} else if ((si = dyn_cast<SwitchInst>(ti))) { //If the termination instruction is a switch instruction.
		condition = si->getCondition();
		//Including the predicate on the predicatesVector.
		predicatesVector.push_back(new Pred(condition));
		pred = predicatesVector.back();

		//Check if some child in CFG is not dominated by node.
		dependentChildrenNotDominated(node, pred, DT, PD);

		//Make control dependence to children in the dominance tree.
		for (unsigned int i = 0; i < nodeTemp->getNumChildren(); ++i) {
			dependentChildren(node, nodeTemp->getChildren()[i]->getBlock(), predicatesVector.back(), PD);
		}
	} else if ((bi = dyn_cast<BranchInst>(ti)) && bi->isUnconditional()) { //If the terminator instruction is not a conditional branch.
		if ((predicate = getPredFromInst(bi)) != NULL) {
			pred = predicate;

			//Check if some child in CFG is not dominated by node.
			dependentChildrenNotDominated(node, pred, DT, PD);

			//Make control dependence to children in the dominance tree.
			for (unsigned int i = 0; i < nodeTemp->getNumChildren(); ++i) {
				dependentChildren(node, nodeTemp->getChildren()[i]->getBlock(), predicate, PD);
			}
		}
	}
}


void bSSA2::dependentChildren(BasicBlock *father, BasicBlock *child, Pred *p,
		PostDominatorTree &PD) {

	BasicBlock *bBSuss = child;
	BasicBlock *temp;

	//If the branch instruction is a constant, it is not related to a basic block. So, return.
	if (dyn_cast<Instruction>(p->getPred()) != NULL)
		temp = dyn_cast<Instruction>(p->getPred())->getParent();
	else {
		return;
	}

	//If the actual basic block post dominates the predicate's basic block.
	if (PD.dominates(bBSuss, temp) && (bBSuss != temp)) {
		return;
	} else {

		//Instruction will be control dependent from bBOring predicate
		for (BasicBlock::iterator bBIt = bBSuss->begin(), bBEnd = bBSuss->end();
				bBIt != bBEnd; ++bBIt) {
			//If is a function call which is defined on the same module.
			if (CallInst *CI = dyn_cast<CallInst>(&(*bBIt))) {
				Function *F = CI->getCalledFunction();
				if (F != NULL)
					if (!F->isDeclaration() && !F->isIntrinsic()) {
						//Make control dependence to function just if it is not control dependent from this predicate.
						if (!p->isFDependent(F))
							dependentFunction(F, p);
					}
			}

			//Make control dependence to other instructions.
			p->addInst(bBIt);

		}
	}
}



void bSSA2::incGraph(Graph *g) {
	unsigned int i;
	int j;

	//For all predicates in predicatesVector
	for (i = 0; i < predicatesVector.size(); ++i) {
		//Locates the predicate (icmp instruction) from the graph,
		GraphNode *predNode = g->findNode(predicatesVector[i]->getPred());

		//For each predicate, iterates on the list of control dependent instructions.
		for (j = 0; j < predicatesVector[i]->getNumInstrucoes(); ++j) {
			GraphNode *instNode = g->findNode(predicatesVector[i]->getInst(j));
			if (predNode != NULL && instNode != NULL) {	//If the instruction is in the graph, make an edge.
				g->addEdge(predNode, instNode, etControl);
			}
		}

		for (j = 0; j < predicatesVector[i]->getNumFunctions(); ++j) {
			Function *F = predicatesVector[i]->getFunc(j);
			g->addJoin(F);
			GraphNode *jn = g->findJoinNode(F);
			g->addEdge(predNode, jn, etControl);
		}

	}

}


void bSSA2::dependentFunction(Function *F, Pred *p) {
	p->addFunc(F);
}


bSSA2::~bSSA2 () {
	src.clear();
	dst.clear();
	srcFileName.clear();
	dstFileName.clear();
	srcLine.clear();
	dstLine.clear();
}

char bSSA2::ID = 0;
static RegisterPass<bSSA2> X("bssa2",
		"B Assignment Construction 2 - Optimized Version");


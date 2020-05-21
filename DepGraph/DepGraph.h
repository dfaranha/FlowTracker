#ifndef DEPGRAPH_H_
#define DEPGRAPH_H_

#define USE_ALIAS_SETS true

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/CallSite.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/DominanceFrontier.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "../AliasSets/AliasSets.h"

#include <deque>
#include <algorithm>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <utility>
#include <iostream>

using namespace std;

namespace llvm {

typedef enum {
        etData = 0, etControl = 1
} edgeType;

/*
 * Class GraphNode
 *
 * This abstract class can do everything a simple graph node can do:
 *              - It knows the nodes that points to it
 *              - It knows the nodes who are ponted by it
 *              - It has a unique ID that can be used to identify the node
 *              - It knows how to connect itself to another GraphNode
 *
 * This class provides virtual methods that makes possible printing the graph
 * in a fancy .dot file, providing for each node:
 *              - Label
 *              - Shape
 *              - Style
 *
 */
class GraphNode {
private:
        std::map<GraphNode*, edgeType> successors;
        std::map<GraphNode*, edgeType> predecessors;

        static int currentID;
        int ID;


protected:
        int Class_ID;
public:
        bool tainted;
        GraphNode* predShortPath; //Store the predecessor in a short path
        bool shortestPath;
        GraphNode();
        GraphNode(GraphNode &G);

        virtual ~GraphNode();

        static inline bool classof(const GraphNode *N) {
                return true;
        }
        ;
        std::map<GraphNode*, edgeType> getSuccessors();
        bool hasSuccessor(GraphNode* succ);

        std::map<GraphNode*, edgeType> getPredecessors();
        bool hasPredecessor(GraphNode* pred);

        void connect(GraphNode* dst, edgeType type = etData);
        int getClass_Id() const;
        int getId() const;
        std::string getName();
        virtual std::string getLabel() = 0;
        virtual std::string getShape() = 0;
        virtual std::string getStyle();

        virtual GraphNode* clone() = 0;
};

/*
 * Class OpNode
 *
 * This class represents the operation nodes:
 *              - It has a OpCode that is compatible with llvm::Instruction OpCodes
 *              - It may or may not store a value, that is the variable defined by the operation
 */
class OpNode: public GraphNode {
private:
        unsigned int OpCode;
        Value* value;
public:
        OpNode(int OpCode);
        OpNode(int OpCode, Value* v);
        ~OpNode();

        static inline bool classof(const GraphNode *N) {
                return N->getClass_Id() == 1 || N->getClass_Id() == 3;
        }

        unsigned int getOpCode() const;
        void setOpCode(unsigned int opCode);
        Value* getValue();

        std::string getLabel();
        std::string getShape();

        GraphNode* clone();
};

/*
 * Class CallNode
 *
 * This class represents operation nodes of llvm::Call instructions:
 *              - It stores the pointer to the called function
 */
class CallNode: public OpNode {
private:
        CallInst* CI;
public:
        CallNode(CallInst* CI) :
                OpNode(Instruction::Call, CI), CI(CI) {
                this->Class_ID = 3;
        }
        ;
        static inline bool classof(const GraphNode *N) {
                return N->getClass_Id() == 3;
        }
        ;
        Function* getCalledFunction() const;
	
	CallInst* getCallInst() const;

        std::string getLabel();
        std::string getShape();

        GraphNode* clone();
};

/*
 * Class VarNode
 *
 * This class represents variables and constants which are not pointers:
 *              - It stores the pointer to the corresponding Value*
 */
class VarNode: public GraphNode {
private:
        Value* value;
public:
        VarNode(Value* value);
        ~VarNode();

        static inline bool classof(const GraphNode *N) {
                return N->getClass_Id() == 2;
        }

        Value* getValue();

        std::string getLabel();
        std::string getShape();

        GraphNode* clone();
};


/*
 * Class JoinNode
 *
 * This class represents a special node which is used as an optimization on bSSA:
 * - All definition in some Functions depends of this node via control edges and this node depends of a predicate
 */
class JoinNode: public GraphNode {
private:
		 Value* value;

public:
	 	JoinNode(Value* value);
	 	~JoinNode();

		static inline bool classof (const GraphNode *N) {
			 return N->getClass_Id() == 5;
		};

		Value *getValue();
		std::string getLabel();
		std::string getShape();
		GraphNode* clone();
};


/*
 * Class VarNode
 *
 * This class represents AliasSets of pointer values:
 *              - It stores the ID of the AliasSet
 *              - It provides a method to get access to all the Values contained in the AliasSet
 */
class MemNode: public GraphNode {
private:
        int aliasSetID;
        AliasSets *AS;
public:
        MemNode(int aliasSetID, AliasSets *AS);
        ~MemNode();

        static inline bool classof(const GraphNode *N) {
                return N->getClass_Id() == 4;
        }

        std::set<Value*> getAliases();

        std::string getLabel();
        std::string getShape();
        GraphNode* clone();
        std::string getStyle();

        int getAliasSetId() const;
};

/*
 * Class Graph
 *
 * Stores a set of nodes. Each node knows how to go to other nodes.
 *
 * The class provides methods to:
 *              - Find specific nodes
 *              - Delete specific nodes
 *              - Print the graph
 *
 */
//Dependence Graph
class Graph {
private:

        llvm::DenseMap<Value*, GraphNode*> opNodes;
        llvm::DenseMap<Value*, GraphNode*> callNodes;

        llvm::DenseMap<Value*, GraphNode*> varNodes;

        llvm::DenseMap<Value*, GraphNode*> joinNodes;

        llvm::DenseMap<int, GraphNode*> memNodes;


        std::set<GraphNode*> nodes;

        AliasSets *AS;

        bool isValidInst(Value *v); //Return true if the instruction is valid for dependence graph construction



public:

        bool isMemoryPointer(Value *v); //Return true if the value is a memory pointer

	typedef std::set<GraphNode*>::iterator iterator;

        std::set<GraphNode*>::iterator begin();
        std::set<GraphNode*>::iterator end();

        //Constructor
        Graph(AliasSets *AS);
        ~Graph(); //Destructor - Free adjacent matrix's memory

        int getTaintedEdges ();
        int getTaintedNodesSize ();

        GraphNode* addInst(Value *v); //Add an instruction into Dependence Graph

        void addEdge(GraphNode* src, GraphNode* dst, edgeType type = etData);

        GraphNode* findNode(Value *op); //Return the pointer to the node or NULL if it is not in the graph
        GraphNode* findNodeOperator(Value *op); //Return the pointer to the node or NULL if it is not in the graph
        GraphNode* findNodeMem(Value *op); //Return the pointer to the node or NULL if it is not in the graph
        std::set<GraphNode*> findNodes(std::set<Value*> values);

        OpNode* findOpNode(Value *op); //Return the pointer to the node or NULL if it is not in the graph

        std::set<GraphNode*> getNodes();

        //print graph in dot format
        class Guider {
        public:
                Guider(Graph* graph);
                std::string getNodeAttrs(GraphNode* n);
                std::string getEdgeAttrs(GraphNode* u, GraphNode* v);
                void setNodeAttrs(GraphNode* n, std::string attrs);
                void setEdgeAttrs(GraphNode* u, GraphNode* v, std::string attrs);
                void clear();
        private:
                Graph* graph;
                DenseMap<GraphNode*, std::string> nodeAttrs;
                DenseMap<std::pair<GraphNode*, GraphNode*>, std::string> edgeAttrs;
        };
        void toDot(std::string s); //print in stdErr
        void toDot(std::string s, std::string fileName); //print in a file
        void toDotLinesPruned(std::string s, const std::string fileName);
        void toDotLinesPruned(std::string s, raw_ostream *stream);
        void toASCIILinesPruned(Value *e, std::string s, const std::string fileName);
        void toASCIILinesPruned(Value *e, std::string s, raw_ostream *stream);
        void toDotLines(std::string s, std::string fileName); //print in a file
        void toDot(std::string s, raw_ostream *stream); //print in any stream
        void toDotLines(std::string s, raw_ostream *stream); //print in any stream
        void toDot(std::string s, raw_ostream *stream, llvm::Graph::Guider* g);


        void bfsVisitMem (GraphNode *s, GraphNode *e);

        Graph generateSubGraph(Value *src, Value *dst); //Take a source value and a destination value and find a Connecting Subgraph from source to destination
        Graph generateSubGraphMem(Value *src, Value *dst); //Take a source value and a destination value and find a Connecting Subgraph from source to destination
        void findTaintedNodes (std::vector<Value *> src, std::vector<Value *> dst); //Take a source value and a destination value and mark all nodes from source to destination
        void findTaintedNodesMem (std::vector<Value *> src, std::vector<Value *> dst);
        void findTaintedNodesSrcScanf (std::vector<Value *> src, std::vector<Value *> dst); //Somente usado quando scanf são as entradas.
        void dfsVisit(GraphNode* u, GraphNode* u2, std::set<GraphNode*> &visitedNodes); //Used by findConnectingSubgraph() method
        void dfsVisitBack(GraphNode* u, GraphNode* u2, std::set<GraphNode*> &visitedNodes); //Used by findConnectingSubgraph() method
        void dfsVisitMemo(GraphNode* u, std::set<GraphNode*> &visitedNodes); //Used by findConnectingSubgraph() method
        void dfsVisitBackMemo(GraphNode* u, std::set<GraphNode*> &visitedNodes); //Used by findConnectingSubgraph() method
        int countTaintedNodes();
        void addJoin (Function *f);
        JoinNode *addJoinInst (Value *v);
        GraphNode *findJoinNode(Value *v);
        bool check0InDegreeFunc (Function *f, std::map<GraphNode*, edgeType> predecessors);

        void deleteCallNodes(Function* F);

        /*
         * Function getNearestDependence
         *
         * Given a sink, returns the nearest source in the graph and the distance to the nearest source
         */
        std::pair<GraphNode*, int> getNearestDependency(Value* sink,
                        std::set<Value*> sources, bool skipMemoryNodes);

        /*
         * Function getEveryDependency
         *
         * Given a sink, returns shortest path to each source (if it exists)
         */
        std::map<GraphNode*, std::vector<GraphNode*> > getEveryDependency(
                        llvm::Value* sink, std::set<llvm::Value*> sources,
                        bool skipMemoryNodes);

        int getNumOpNodes();
        int getNumCallNodes();
        int getNumMemNodes();
        int getNumVarNodes();
        int getNumDataEdges();
        int getNumControlEdges();
        int getNumEdges(edgeType type);
};

/*
 * Class functionDepGraph
 *
 * Function pass that provides an intraprocedural dependency graph
 *
 */
class FunctionDepGraph: public FunctionPass {
public:
        static char ID; // Pass identification, replacement for typeid.
        FunctionDepGraph() :
                FunctionPass(ID), depGraph(NULL) {
        }
        void getAnalysisUsage(AnalysisUsage &AU) const;
        bool runOnFunction(Function&);

        Graph* depGraph;
};

/*
 * Class moduleDepGraph
 *
 * Module pass that provides a context-insensitive interprocedural dependency graph
 *
 */
class ModuleDepGraph: public ModulePass {
public:
        static char ID; // Pass identification, replacement for typeid.
        ModuleDepGraph() :
                ModulePass(ID), depGraph(NULL) {
        }
        void getAnalysisUsage(AnalysisUsage &AU) const;
        bool runOnModule(Module&);

        void matchParametersAndReturnValues(Function &F);
        void deleteCallNodes(Function* F);

        Graph* depGraph;
};

class ViewModuleDepGraph: public ModulePass {
public:
        static char ID; // Pass identification, replacement for typeid.
        ViewModuleDepGraph() :
                ModulePass(ID) {
        }

        void getAnalysisUsage(AnalysisUsage &AU) const {
                AU.addRequired<ModuleDepGraph> ();
                AU.setPreservesAll();
        }

        bool runOnModule(Module& M) {

                ModuleDepGraph& DepGraph = getAnalysis<ModuleDepGraph> ();
                Graph *g = DepGraph.depGraph;


                //Stats from dependence graph
                errs()<<"Var Nodes " << g->getNumVarNodes()<<" \n";
                errs()<<"Op Nodes " <<g->getNumOpNodes()<<" \n";
                errs()<<"Mem Nodes " <<g->getNumMemNodes()<<" \n";
                errs()<<"Call Nodes" <<g->getNumCallNodes()<<" \n";
                errs()<<"Data Edges " <<g->getNumDataEdges()<<" \n";
                //errs()<<"Control Edges " <<g->getNumControlEdges()<<" \n";
                std::set<GraphNode*> s = g->getNodes();
                errs()<<"Nodes Size "<<s.size()<<"\n";

                std::string tmp = M.getModuleIdentifier();
                replace(tmp.begin(), tmp.end(), '\\', '_');

                std::string Filename = "/tmp/" + tmp + ".dot";

                //Print dependency graph (in dot format)
                g->toDot(M.getModuleIdentifier(), Filename);

                //DisplayGraph(PathFilename, true, GraphProgram::DOT);

                return false;
        }
};
}

#endif //DEPGRAPH_H_

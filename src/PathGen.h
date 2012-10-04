#pragma once

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include "SMTSolver.h"

namespace llvm {
	class BasicBlock;
	class BranchInst;
	class DominatorTree;
	class SwitchInst;
	class TerminatorInst;
} // namespace llvm

class ValueGen;

class PathGen {
public:
	typedef llvm::DenseMap<llvm::BasicBlock *, SMTExpr> BBExprMap;
	typedef BBExprMap::iterator iterator;
	typedef std::pair<const llvm::BasicBlock *, const llvm::BasicBlock *> Edge;
	typedef llvm::SmallVectorImpl<Edge> EdgeVec;

	PathGen(ValueGen &, const EdgeVec &);
	PathGen(ValueGen &, const EdgeVec &, llvm::DominatorTree &DT);
	~PathGen();

	SMTExpr get(llvm::BasicBlock *);

private:
	ValueGen &VG;
	const EdgeVec &Backedges;
	llvm::DominatorTree *DT;
	BBExprMap Cache;

	bool isBackedge(llvm::BasicBlock *, llvm::BasicBlock *);
	SMTExpr getTermGuard(llvm::TerminatorInst *I, llvm::BasicBlock *BB);
	SMTExpr getTermGuard(llvm::BranchInst *I, llvm::BasicBlock *BB);
	SMTExpr getTermGuard(llvm::SwitchInst *I, llvm::BasicBlock *BB);
	SMTExpr getPHIGuard(llvm::BasicBlock *BB, llvm::BasicBlock *Pred);
};

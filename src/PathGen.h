#pragma once

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallVector.h>
#include "SMTSolver.h"

namespace llvm {
	class BasicBlock;
	class BranchInst;
	class SwitchInst;
	class TerminatorInst;
} // namespace llvm

class ValueGen;

class PathGen {
	ValueGen &VG;

	typedef llvm::DenseMap<llvm::BasicBlock *, SMTExpr> BBExprMap;
	typedef BBExprMap::iterator iterator;
	BBExprMap Cache;

	typedef std::pair<
		const llvm::BasicBlock *, const llvm::BasicBlock *
	> Edge;
	llvm::SmallVector<Edge, 16> BackEdges;

	SMTExpr getTermGuard(llvm::TerminatorInst *I, llvm::BasicBlock *BB);
	SMTExpr getTermGuard(llvm::BranchInst *I, llvm::BasicBlock *BB);
	SMTExpr getTermGuard(llvm::SwitchInst *I, llvm::BasicBlock *BB);
	SMTExpr getPHIGuard(llvm::BasicBlock *BB, llvm::BasicBlock *Pred);

public:
	PathGen(ValueGen &);
	~PathGen();

	SMTExpr get(llvm::BasicBlock *);
};

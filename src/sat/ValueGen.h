#pragma once

#include <llvm/ADT/DenseMap.h>
#include <llvm/IR/DataLayout.h>
#include "SMTSolver.h"

class ValueGen {
public:
	llvm::DataLayout &TD;
	SMTSolver &SMT;

	typedef llvm::DenseMap<llvm::Value *, SMTExpr> ValueExprMap;
	typedef ValueExprMap::iterator iterator;
	ValueExprMap Cache;

	ValueGen(llvm::DataLayout &, SMTSolver &);
	~ValueGen();

	static bool isAnalyzable(llvm::Value *);
	static bool isAnalyzable(llvm::Type *);
	SMTExpr get(llvm::Value *);

	iterator begin() { return Cache.begin(); }
	iterator end() { return Cache.end(); }
};

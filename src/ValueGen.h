#pragma once

#include <llvm/ADT/DenseMap.h>
#include "SMTSolver.h"

namespace llvm {
	class TargetData;
	class Value;
} // namespace llvm

class ValueGen {
public:
	llvm::TargetData &TD;
	SMTSolver &SMT;

	typedef llvm::DenseMap<llvm::Value *, SMTExpr> ValueExprMap;
	typedef ValueExprMap::iterator iterator;
	ValueExprMap Cache;

	ValueGen(llvm::TargetData &, SMTSolver &);
	~ValueGen();

	SMTExpr get(llvm::Value *);

};

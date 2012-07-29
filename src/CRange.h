#pragma once

#include <llvm/Support/ConstantRange.h>

// llvm::ConstantRange fixup.
class CRange : public llvm::ConstantRange {
	typedef llvm::APInt APInt;
	typedef llvm::ConstantRange super;
	CRange(uint32_t BitWidth, bool isFullSet) : super(BitWidth, isFullSet) {}
public:
	// Constructors.
	CRange(const super &CR) : super(CR) {}
	CRange(const APInt &Value)
		: super(Value) {}
	CRange(const APInt &Lower, const APInt &Upper)
		: super(Lower, Upper) {}
	static CRange makeFullSet(uint32_t BitWidth) {
		return CRange(BitWidth, true);
	}
	static CRange makeEmptySet(uint32_t BitWidth) {
		return CRange(BitWidth, false);
	}
	static CRange makeICmpRegion(unsigned Pred, const CRange &other) {
		return super::makeICmpRegion(Pred, other);
	}

	CRange sadd(const CRange &RHS) const {
		unsigned N = getBitWidth();
		if (isEmptySet() || RHS.isEmptySet())
			return makeEmptySet(N);
		APInt NewLower = getLower().sext(N + 1) + RHS.getLower().sext(N + 1);
		APInt NewUpper = (getUpper() - 1).sext(N + 1) + (RHS.getUpper() - 1).sext(N + 1) + 1;
		// Intersect with [INT_MIN, INT_MAX].
		return CRange(NewLower, NewUpper)
			.intersectWith(makeFullSet(N).signExtend(N + 1))
			.truncate(N);
	}

	CRange sdiv(const CRange &RHS) const {
		if (isEmptySet() || RHS.isEmptySet())
			return makeEmptySet(getBitWidth());
		// FIXME: too conservative.
		return makeFullSet(getBitWidth());
	}
};

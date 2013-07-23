#pragma once

#include <llvm/Support/Compiler.h>
#include <llvm/Support/ConstantRange.h>
#include <llvm/Support/Debug.h>

// llvm::ConstantRange fixup.
class CRange : public llvm::ConstantRange {
	typedef llvm::ConstantRange super;
	// duplicate private APIntMoveTy from ConstantRange
#if LLVM_HAS_RVALUE_REFERENCES
	typedef llvm::APInt APIntMoveTy;
#else
	typedef const llvm::APInt &APIntMoveTy;
#endif
public:
	CRange(uint32_t BitWidth, bool isFullSet) : super(BitWidth, isFullSet) {}
	// Constructors.
	CRange(const super &CR) : super(CR) {}
	CRange(APIntMoveTy Value) : super(Value) {}
	CRange(APIntMoveTy Lower, APIntMoveTy Upper) : super(Lower, Upper) {}
	static CRange makeFullSet(uint32_t BitWidth) {
		return CRange(BitWidth, true);
	}
	static CRange makeEmptySet(uint32_t BitWidth) {
		return CRange(BitWidth, false);
	}
	static CRange makeICmpRegion(unsigned Pred, const CRange &other) {
		return super::makeICmpRegion(Pred, other);
	}

	void match(const CRange &R) {
		if (this->getBitWidth() != R.getBitWidth()) {
			llvm::dbgs() << "warning: range " << *this << " " 
				<< this->getBitWidth() << " and " << R << " "
				<< R.getBitWidth() << " unmatch\n";
			*this = this->zextOrTrunc(R.getBitWidth());
		}
	}

	bool safeUnion(const CRange &R) {
		CRange V = R, Old = *this;
		V.match(*this);
		*this = this->unionWith(V);
		return Old != *this;
	}


	CRange sdiv(const CRange &RHS) const {
		if (isEmptySet() || RHS.isEmptySet())
			return makeEmptySet(getBitWidth());
		// FIXME: too conservative.
		return makeFullSet(getBitWidth());
	}

};

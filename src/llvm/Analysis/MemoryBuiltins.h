#pragma once

#include <llvm/Config/config.h>
#include_next <llvm/Analysis/MemoryBuiltins.h>

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2
namespace llvm {

class TargetLibraryInfo;

static inline CallInst *isFreeCall(Value *I, TargetLibraryInfo *) {
	return isFreeCall(I);
}

} // namespace llvm
#endif

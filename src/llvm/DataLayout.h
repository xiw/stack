#pragma once

#include <llvm/Config/config.h>

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2
#include <llvm/TargetData/TargetData.h>
namespace llvm {
	typedef TargetData DataLayout;
}
#else
#include_next <llvm/DataLayout.h>
#endif

#pragma once

#include <llvm/Config/config.h>

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2
#include <llvm/Analysis/DebugInfo.h>
#else
#include_next <llvm/DebugInfo.h>
#endif

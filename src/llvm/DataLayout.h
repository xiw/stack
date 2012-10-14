#pragma once

#include <llvm/Config/config.h>

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 2
#include <llvm/Target/TargetData.h>
#define DataLayout TargetData
#else
#include_next <llvm/DataLayout.h>
#endif

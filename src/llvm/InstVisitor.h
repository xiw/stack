#pragma once

#include <llvm/Config/config.h>

#if LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR < 3
#include <llvm/Support/InstVisitor.h>
#else
#include_next <llvm/InstVisitor.h>
#endif

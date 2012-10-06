#include <config.h>

#if defined(HAVE_LLVM_IRBUILDER_H)
#include <llvm/IRBuilder.h>
#elif defined(HAVE_LLVM_SUPPORT_IRBUILDER_H)
#include <llvm/Support/IRBuilder.h>
#else
#error IRBuilder.h not found
#endif

#if defined(HAVE_LLVM_DEBUGINFO_H)
#include <llvm/DebugInfo.h>
#elif defined(HAVE_LLVM_ANALYSIS_DEBUGINFO_H)
#include <llvm/Analysis/DebugInfo.h>
#else
#error DebugInfo.h not found
#endif

#pragma once

#include <llvm/ADT/StringMap.h>

namespace llvm {
	class DebugLoc;
	class LLVMContext;
	class MemoryBuffer;
} // namespace llvm

class FileCache {
public:
	~FileCache();
	llvm::StringRef getLine(llvm::StringRef Filename, unsigned Line);
	llvm::StringRef getLine(const llvm::DebugLoc &, llvm::LLVMContext &);
private:
	typedef llvm::StringMap<llvm::MemoryBuffer *> BufferMap;
	BufferMap BM;
};

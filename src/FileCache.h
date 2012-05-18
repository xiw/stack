#pragma once

#include <llvm/ADT/StringMap.h>

namespace llvm {
	class DebugLoc;
	class LLVMContext;
	class MDNode;
	class MemoryBuffer;
	template <typename T> class SmallVectorImpl;
} // namespace llvm

class FileCache {
public:
	~FileCache();
	llvm::StringRef getFile(llvm::StringRef Filename);
	llvm::StringRef getFile(const llvm::MDNode *);
	llvm::StringRef getLine(const llvm::DebugLoc &, llvm::LLVMContext &);
	static void getPath(llvm::SmallVectorImpl<char> &, const llvm::MDNode *);
private:
	typedef llvm::StringMap<llvm::MemoryBuffer *> BufferMap;
	BufferMap BM;
};

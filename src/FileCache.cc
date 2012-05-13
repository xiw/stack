#include "FileCache.h"
#include <llvm/ADT/OwningPtr.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/Support/DebugLoc.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/system_error.h>

using namespace llvm;

FileCache::~FileCache() {
	for (BufferMap::iterator i = BM.begin(), e = BM.end(); i != e; ++i)
		delete i->second;
}

StringRef FileCache::getLine(StringRef Filename, unsigned Line) {
	MemoryBuffer *&MB = BM[Filename];
	if (!MB) {
		OwningPtr<MemoryBuffer> Result;
		MemoryBuffer::getFile(Filename, Result);
		if (!Result)
			return StringRef();
		MB = Result.take();
	}
	StringRef First, Second = MB->getBuffer();
	for (unsigned i = 0; i != Line; ++i)
		tie(First, Second) = Second.split('\n');
	return First;
}

StringRef FileCache::getLine(const DebugLoc &DbgLoc, LLVMContext &VMCtx) {
	MDNode *MD = DbgLoc.getAsMDNode(VMCtx);
	if (!MD)
		return StringRef();
	DILocation Loc(MD);
	unsigned Line = Loc.getLineNumber();
	if (!Line)
		return StringRef();
	std::string Path = (Loc.getDirectory() + Loc.getFilename()).str();
	return getLine(Path, Line);
}

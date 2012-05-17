#include "FileCache.h"
#include <llvm/ADT/OwningPtr.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/Support/DebugLoc.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/system_error.h>

using namespace llvm;

FileCache::~FileCache() {
	for (BufferMap::iterator i = BM.begin(), e = BM.end(); i != e; ++i)
		delete i->second;
}

StringRef FileCache::getFile(llvm::StringRef Filename) {
	MemoryBuffer *&MB = BM[Filename];
	if (!MB) {
		OwningPtr<MemoryBuffer> Result;
		MemoryBuffer::getFile(Filename, Result);
		if (!Result)
			return StringRef();
		MB = Result.take();
	}
	return MB->getBuffer();
}

StringRef FileCache::getFile(const MDNode *MD) {
	DICompileUnit CU(MD);
	if (!CU.Verify())
		return StringRef();
	SmallString<64> Path;
	sys::path::append(Path, CU.getDirectory(), CU.getFilename());
	return getFile(Path);
}

StringRef FileCache::getLine(StringRef Filename, unsigned Line) {
	StringRef First, Second = getFile(Filename);
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

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

StringRef FileCache::getFile(StringRef Filename) {
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
	SmallString<64> Path;
	getPath(Path, MD);
	return getFile(Path);
}

StringRef FileCache::getLine(const DebugLoc &DbgLoc, LLVMContext &VMCtx) {
	StringRef Str = getFile(DbgLoc.getScope(VMCtx));
	unsigned LineNo = DbgLoc.getLine();
	StringRef Line;
	for (unsigned i = 0; i != LineNo; ++i)
		tie(Line, Str) = Str.split('\n');
	return Line;
}

void FileCache::getPath(SmallVectorImpl<char> &Path, const MDNode *MD) {
	StringRef Filename = DIScope(MD).getFilename();
	if (sys::path::is_absolute(Filename))
		Path.append(Filename.begin(), Filename.end());
	else
		sys::path::append(Path, DIScope(MD).getDirectory(), Filename);
}

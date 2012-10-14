#include "llvm/LLVMContext.h"
#include "llvm/PassManager.h"
#include "llvm/Module.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Path.h"
#include <memory>
#include <vector>

#include "IntGlobal.h"
#include "Annotation.h"

using namespace llvm;

static cl::list<std::string>
InputFilenames(cl::Positional, cl::OneOrMore,
               cl::desc("<input bitcode files>"));

static cl::opt<bool>
Verbose("v", cl::desc("Print information about actions taken"));

static cl::opt<bool>
NoWriteback("p", cl::desc("Do not writeback annotated bytecode"));

ModuleList Modules;
GlobalContext GlobalCtx;

#define Diag if (Verbose) llvm::errs()

void doWriteback(Module *M, StringRef name)
{
	std::string err;
	OwningPtr<tool_output_file> out(
		new tool_output_file(name.data(), err, raw_fd_ostream::F_Binary));
	if (!err.empty()) {
		Diag << "Cannot write back to " << name << ": " << err << "\n";
		return;
	}
	M->print(out->os(), NULL);
	out->keep();
}


void IterativeModulePass::run(ModuleList &modules) {

	ModuleList::iterator i, e;
	Diag << "[" << ID << "] Initializing " << modules.size() << " modules ";
	for (i = modules.begin(), e = modules.end(); i != e; ++i) {
		doInitialization(i->first);
		Diag << ".";
	}
	Diag << "\n";

	unsigned iter = 0, changed = 1;
	while (changed) {
		++iter;
		changed = 0;
		for (i = modules.begin(), e = modules.end(); i != e; ++i) {
			Diag << "[" << ID << " / " << iter << "] ";
			Diag << "'" << i->first->getModuleIdentifier() << "'";

			bool ret = doModulePass(i->first);
			if (ret) {
				++changed;
				Diag << " [CHANGED]\n";
			} else
				Diag << "\n";
		}
		Diag << "[" << ID << "] Updated in " << changed << " modules.\n";
	}

	Diag << "\n[" << ID << "] Postprocessing ...\n";
	for (i = modules.begin(), e = modules.end(); i != e; ++i) {
		if (doFinalization(i->first) && !NoWriteback) {
			Diag << "[" << ID << "] Writeback " << i->second << "\n";
			doWriteback(i->first, i->second);
		}
	}
			
	Diag << "[" << ID << "] Done!\n";
}

int main(int argc, char **argv)
{
	// Print a stack trace if we signal out.
	sys::PrintStackTraceOnErrorSignal();
	PrettyStackTraceProgram X(argc, argv);

	llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
	cl::ParseCommandLineOptions(argc, argv, "global analysis\n");
	SMDiagnostic Err;	
	
	// Loading modules
	Diag << "Total " << InputFilenames.size() << " file(s)\n";

	for (unsigned i = 0; i < InputFilenames.size(); ++i) {
		// use separate LLVMContext to avoid type renaming
		LLVMContext *LLVMCtx = new LLVMContext();
		Module *M = ParseIRFile(InputFilenames[i], Err, *LLVMCtx);

		if (M == NULL) {
			errs() << argv[0] << ": error loading file '" 
				<< InputFilenames[i] << "'\n";
			continue;
		}

		Diag << "Loading '" << InputFilenames[i] << "'\n";

		// annotate
		static AnnotationPass AnnoPass;
		AnnoPass.doInitialization(*M);
		for (Module::iterator j = M->begin(), je = M->end(); j != je; ++j)
			AnnoPass.runOnFunction(*j);
		if (!NoWriteback)
			doWriteback(M, InputFilenames[i].c_str());

		Modules.push_back(std::make_pair(M, InputFilenames[i]));
	}

	// Main workflow
	CallGraphPass CGPass(&GlobalCtx);
	CGPass.run(Modules);

	TaintPass TPass(&GlobalCtx);
	TPass.run(Modules);

	RangePass RPass(&GlobalCtx);
	RPass.run(Modules);

	if (NoWriteback) {
		TPass.dumpTaints();
		RPass.dumpRange();
	}

	return 0;
}


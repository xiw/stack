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
Writeback("u", cl::desc("Write back annotated bitcode"));

ModuleList Modules;
GlobalContext GlobalCtx;

#define Diag if (Verbose) llvm::errs()

void IterativeModulePass::run(ModuleList &modules) {

	ModuleList::iterator i, e;
	Diag << "[" << ID << "] Initializing " << modules.size() << " modules ";
	for (i = modules.begin(), e = modules.end(); i != e; ++i) {
		doInitialization(i->first);
		Diag << ".";
	}

	unsigned iter = 0, changed = 1;
	while (changed) {
		++iter;
		changed = 0;
		for (i = modules.begin(), e = modules.end(); i != e; ++i) {
			Diag << "\n\n[" << ID << " / " << iter << "] ";
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
	for (i = modules.begin(), e = modules.end(); i != e; ++i)
		doFinalization(i->first, i->second);
	Diag << "[" << ID << "] Done!\n";
}

void doWriteback(Module *M, StringRef name)
{
	if (Writeback) {
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
}

int main(int argc, char **argv)
{
	// Print a stack trace if we signal out.
	sys::PrintStackTraceOnErrorSignal();
	PrettyStackTraceProgram X(argc, argv);

	llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
	cl::ParseCommandLineOptions(argc, argv, "global analysis\n", true);
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
		doWriteback(M, InputFilenames[i].c_str());

		Modules.push_back(std::make_pair(M, InputFilenames[i]));
	}
	
	// Main workflow
	CallGraphPass CGPass(&GlobalCtx);
	CGPass.run(Modules);

	//CGPass.dumpFuncPtrs();
	//CGPass.dumpCallees();

	TaintPass TPass(&GlobalCtx);
	TPass.run(Modules);
	//TPass.dumpTaints();

	RangePass RPass(&GlobalCtx);
	RPass.run(Modules);
	RPass.dumpRange();

	return 0;
}


#define DEBUG_TYPE "int-div"
#include <llvm/IRBuilder.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/Support/InstIterator.h>

using namespace llvm;

namespace {

struct IntDiv : FunctionPass {
	static char ID;
	IntDiv() : FunctionPass(ID) { }

	virtual bool runOnFunction(Function &);

private:
	Value *insertDivCheck(Value *);
};

} // anonymous namespace

void insertIntSat(Value *, Instruction *);

bool IntDiv::runOnFunction(Function &F) {
	bool Changed = false;
	for (inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i) {
		Instruction *I = &*i;
		switch (I->getOpcode()) {
		default: continue;
		case Instruction::SDiv:
		case Instruction::UDiv:
			break;
		}
		IRBuilder<> Builder(I);
		Value *V = Builder.CreateIsNull(I->getOperand(1));
		insertIntSat(V, I);
		Changed = true;
	}
	return Changed;
}

char IntDiv::ID;

static RegisterPass<IntDiv>
X("int-div", "Insert divisor zero checks", false, false);

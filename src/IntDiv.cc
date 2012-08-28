#define DEBUG_TYPE "int-div"
#include <llvm/IRBuilder.h>
#include <llvm/Module.h>
#include <llvm/Pass.h>
#include <llvm/ADT/SmallVector.h>
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

void insertIntTrap(Value *V, StringRef Anno, Instruction *IP, Pass *P);

bool IntDiv::runOnFunction(Function &F) {
	SmallVector<std::pair<Value *, Instruction *>, 4> Checks;
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
		Checks.push_back(std::make_pair(V, I));
	}
	// Since inserting trap will change the control flow, it's better
	// to do it after looping over all instructions.
	for (size_t i = 0, n = Checks.size(); i != n; ++i) {
		Value *V = Checks[i].first;
		Instruction *I = Checks[i].second;
		insertIntTrap(V, I->getOpcodeName(), I, this);
	}
	return !Checks.empty();
}

char IntDiv::ID;

static RegisterPass<IntDiv>
X("int-div", "Insert divisor zero checks", false, false);

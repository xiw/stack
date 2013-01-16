#include <llvm/Support/CommandLine.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <err.h>
#include <unistd.h>

using namespace llvm;

static cl::opt<unsigned>
SMTTimeoutOpt("smt-timeout",
              cl::desc("Specify a timeout for SMT solver"),
              cl::value_desc("milliseconds"));

static pid_t pid;

int SMTFork()
{
	if (!SMTTimeoutOpt)
		return 0;
	pid = fork();
	if (pid < 0)
		err(1, "fork");
	// Parent process.
	if (pid)
		return 1;
	// Child process.
	struct itimerval itv = {{0, 0}, {(time_t)SMTTimeoutOpt / 1000, (suseconds_t)SMTTimeoutOpt % 1000 * 1000}};
	setitimer(ITIMER_VIRTUAL, &itv, NULL);
	return 0;
}

void SMTJoin(int *status)
{
	if (!SMTTimeoutOpt)
		return;
	// Child process.
	if (pid == 0)
		_exit(*status);
	// Parent process.
	waitpid(pid, status, 0);
	if (WIFEXITED(*status))
		*status = WEXITSTATUS(*status);
	else
		*status = -1;
}

#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <llvm/Pass.h>
#include <llvm/Support/CommandLine.h>
#include "config.h"

using namespace llvm;

static cl::opt<unsigned>
GlobalTimeoutOpt("global-timeout-sec",
		 cl::desc("Specify a global timeout for entire analysis"),
		 cl::value_desc("seconds"));

namespace {

#ifdef HAVE_TIMER
static void global_check(union sigval) {
	struct rusage ru_self, ru_child;
	getrusage(RUSAGE_SELF, &ru_self);
	getrusage(RUSAGE_CHILDREN, &ru_child);
	if (ru_self.ru_utime.tv_sec + ru_child.ru_utime.tv_sec > (time_t)GlobalTimeoutOpt) {
		printf("Global timeout: self %ld.%06ld, child %ld.%06ld\n",
			(long)ru_self.ru_utime.tv_sec, (long)ru_self.ru_utime.tv_usec,
			(long)ru_child.ru_utime.tv_sec, (long)ru_child.ru_utime.tv_usec);
		exit(0);
	}
}
#endif

struct GlobalTimeout : ImmutablePass {
	static char ID;
	GlobalTimeout() : ImmutablePass(ID) {
		if (!GlobalTimeoutOpt)
			return;
#ifdef HAVE_TIMER
		timer_t tid;
		struct sigevent sigev;
		sigev.sigev_notify = SIGEV_THREAD;
		sigev.sigev_notify_function = &global_check;
		sigev.sigev_notify_attributes = nullptr;
		int r = timer_create(CLOCK_MONOTONIC, &sigev, &tid);
		if (r < 0) {
			perror("timer_create");
			return;
		}

		struct itimerspec its;
		its.it_value.tv_sec = 60;
		its.it_value.tv_nsec = 0;
		its.it_interval = its.it_value;
		r = timer_settime(tid, 0, &its, nullptr);
		if (r < 0) {
			perror("timer_settime");
			return;
		}
#endif
	}
};

} // anonymous namespace

char GlobalTimeout::ID;

static RegisterPass<GlobalTimeout>
X("enable-global-timeout", "Terminate process after a fixed timeout (including child time)");

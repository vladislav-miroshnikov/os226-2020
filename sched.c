#include "sched.h"

struct task {
	void (*entry)(void *ctx);
	void *ctx;
	int priority;
	int deadline;
	// ...
};

static struct task taskpool[16];
static int taskpool_n;

void sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority,
		int deadline) {
	struct task *t = &taskpool[taskpool_n++];
	// ...
}

void sched_cont(void (*entrypoint)(void *aspace),
		void *aspace,
		int timeout) {
	// ...
}

void sched_time_elapsed(unsigned amount) {
	// ...
}

void sched_set_policy(enum policy policy) {
	// ...
}

void sched_run(void) {
	// ...
}

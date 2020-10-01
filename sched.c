#include <limits.h>
#include <string.h>
#include <stdio.h>

#include "sched.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

struct task {
	void (*entry)(void *as);
	void *as;
	int priority;
	int deadline;

	// timeout support
	int waketime;

	// policy support
	struct task *next;
};

static int time;

static struct task *current;
static struct task *runq;
static struct task *waitq;

static int (*policy_cmp)(struct task *t1, struct task *t2);

static struct task taskpool[16];
static int taskpool_n;

static void policy_run(struct task *t) {
	struct task **c = &runq;

	while (*c && policy_cmp(*c, t) <= 0) {
		c = &(*c)->next;
	}
	t->next = *c;
	*c = t;
}

void sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority,
		int deadline) {

	if (ARRAY_SIZE(taskpool) <= taskpool_n) {
		fprintf(stderr, "No mem for new task\n");
		return;
	}
	struct task *t = &taskpool[taskpool_n++];

	t->entry = entrypoint;
	t->as = aspace;
	t->priority = priority;
	t->deadline = 0 < deadline ? deadline : INT_MAX;

	policy_run(t);
}

void sched_cont(void (*entrypoint)(void *aspace),
		void *aspace,
		int timeout) {

	if (current->next != current) {
		fprintf(stderr, "Mulitiple sched_cont\n");
		return;
	}

	if (!timeout) {
		policy_run(current);
		return;
	}

	current->waketime = time + timeout;

	struct task **c = &waitq;
	while (*c && (*c)->waketime < current->waketime) {
		c = &(*c)->next;
	}
	current->next = *c;
	*c = current;
}

void sched_time_elapsed(unsigned amount) {
	time += amount;

	while (waitq && waitq->waketime <= time) {
		struct task *t = waitq;
		waitq = waitq->next;
		policy_run(t);
	}
}

static int fifo_cmp(struct task *t1, struct task *t2) {
	return -1;
}

static int prio_cmp(struct task *t1, struct task *t2) {
	return t2->priority - t1->priority;
}

static int deadline_cmp(struct task *t1, struct task *t2) {
	int d = t1->deadline - t2->deadline;
	if (d) {
		return d;
	}
	return prio_cmp(t1, t2);
}

void sched_set_policy(enum policy policy) {
	int (*policies[])(struct task *t1, struct task *t2) = { fifo_cmp, prio_cmp, deadline_cmp };
	policy_cmp = policies[policy];
}

void sched_run(void) {
	if (!policy_cmp) {
		fprintf(stderr, "Policy unset\n");
		return;
	}

	while (runq) {
		current = runq;
		runq = current->next;
		current->next = current;

		current->entry(current->as);
	}
}

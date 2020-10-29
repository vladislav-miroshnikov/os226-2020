#define _GNU_SOURCE

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "timer.h"
#include "sched.h"
#include "ctx.h"
#include "vm.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

/* AMD64 Sys V ABI, 3.2.2 The Stack Frame:
The 128-byte area beyond the location pointed to by %rsp is considered to
be reserved and shall not be modified by signal or interrupt handlers */
#define SYSV_REDST_SZ 128

extern void tramptramp(void);

static void bottom(void);

static volatile int time;
static int tick_period;

static struct task *current;
static int current_start;
static struct task *runq;
static struct task *waitq;

static struct task idle;
static struct task taskpool[16];
static int taskpool_n;

static sigset_t irqs;

static void irq_disable(void) {
	sigprocmask(SIG_BLOCK, &irqs, NULL);
}

static void irq_enable(void) {
	sigprocmask(SIG_UNBLOCK, &irqs, NULL);
}

static int prio_cmp(struct task *t1, struct task *t2) {
	return t1->priority - t2->priority;
}

static void policy_run(struct task *t) {
	struct task **c = &runq;

	while (*c && prio_cmp(*c, t) <= 0) {
		c = &(*c)->next;
	}
	t->next = *c;
	*c = t;
}

static void hctx_push(greg_t *regs, unsigned long val) {
	regs[REG_RSP] -= sizeof(unsigned long);
	*(unsigned long *) regs[REG_RSP] = val;
}

static void top(int sig, siginfo_t *info, void *ctx) {
	ucontext_t *uc = (ucontext_t *) ctx;
	greg_t *regs = uc->uc_mcontext.gregs;

	unsigned long oldsp = regs[REG_RSP];
	regs[REG_RSP] -= SYSV_REDST_SZ;
	hctx_push(regs, regs[REG_RIP]);
	hctx_push(regs, sig);
	hctx_push(regs, regs[REG_RBP]);
	hctx_push(regs, oldsp);
	hctx_push(regs, (unsigned long) bottom);
	regs[REG_RIP] = (greg_t) tramptramp;
}

struct task *sched_current(void) {
	return current;
}

int sched_gettime(void) {
	int cnt1 = timer_cnt();
	int time1 = time;
	int cnt2 = timer_cnt();
	int time2 = time;

	return (cnt1 <= cnt2) ?
		time1 + cnt2 :
		time2 + cnt2;
}

static void doswitch(void) {
	struct task *old = current;
	current = runq;
	runq = current->next;

	current_start = sched_gettime();
	ctx_switch(&old->ctx, &current->ctx);

	vmapplymap();
}

static void tasktramp(void) {
	irq_enable();
	current->entry(current->as);
	irq_disable();
	doswitch();
}

void sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority) {

	if (ARRAY_SIZE(taskpool) <= taskpool_n) {
		fprintf(stderr, "No mem for new task\n");
		return;
	}
	struct task *t = &taskpool[taskpool_n++];

	t->entry = entrypoint;
	t->as = aspace;
	t->priority = priority;

	ctx_make(&t->ctx, tasktramp, t->stack, sizeof(t->stack));

	irq_disable();
	policy_run(t);
	irq_enable();
}

static void bottom(void) {
	time += tick_period;

	while (waitq && waitq->waketime <= sched_gettime()) {
		struct task *t = waitq;
		waitq = waitq->next;
		policy_run(t);
	}

	if (tick_period <= sched_gettime() - current_start) {
		irq_disable();
		policy_run(current);
		doswitch();
		irq_enable();
	}
}

void sched_sleep(unsigned ms) {

#if 0
	if (!ms) {
		irq_disable();
		policy_run(current);
		doswitch();
		irq_enable();
		return;
	}
#endif

	current->waketime = sched_gettime() + ms;

	int curtime;
	while ((curtime = sched_gettime()) < current->waketime) {
		irq_disable();
		struct task **c = &waitq;
		while (*c && (*c)->waketime < current->waketime) {
			c = &(*c)->next;
		}
		current->next = *c;
		*c = current;

		doswitch();
		irq_enable();
	}
}

void sched_run(int period_ms) {
	sigemptyset(&irqs);
	sigaddset(&irqs, SIGALRM);

	tick_period = period_ms;
	timer_init_period(period_ms, top);

	vminit(VM_PAGESIZE * 1024);

	sigset_t none;
	sigemptyset(&none);

	irq_disable();

	current = &idle;

	while (runq || waitq) {
		if (runq) {
			policy_run(current);
			doswitch();
		} else {
			sigsuspend(&none);
		}
	}

	irq_enable();
}

#define _GNU_SOURCE

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <unistd.h>
#include <fcntl.h>
#include <elf.h>
#include <sys/mman.h>

#include "kernel.h"
#include "timer.h"
#include "sched.h"
#include "ctx.h"
#include "vm.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*a))

/* AMD64 Sys V ABI, 3.2.2 The Stack Frame:
The 128-byte area beyond the location pointed to by %rsp is considered to
be reserved and shall not be modified by signal or interrupt handlers */
#define SYSV_REDST_SZ 128

extern int init(int argc, char* argv[]);
extern void exittramp(void);

static void timer_bottom(struct hctx *hctx);

struct task {
	char stack[8192];

	struct vmctx vmctx;
	union {
		struct ctx ctx;
		struct {
			int(*main)(int, char**);
			int argc;
			char **argv;
		};
	};

	int priority;

	// timeout support
	int waketime;

	// policy support
	struct task *next;
} __attribute__((aligned(16)));

static volatile int time;
static int tick_period;

static struct task *current;
static int current_start;
static struct task *runq;
static struct task *waitq;

static struct task idle;
static struct task taskpool[16];
static void inittramp(void);
static int taskpool_n;

static sigset_t irqs;

void irq_disable(void) {
	sigprocmask(SIG_BLOCK, &irqs, NULL);
}

void irq_enable(void) {
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

static void hctx_call(greg_t *regs, void (*bottom)(struct hctx*)) {
	unsigned long *savearea = (unsigned long *)(regs[REG_RSP] - SYSV_REDST_SZ);

	*--savearea = regs[REG_RIP];
	*--savearea = regs[REG_EFL];
	*--savearea = regs[REG_RBP];
	*--savearea = regs[REG_R15];
	*--savearea = regs[REG_R14];
	*--savearea = regs[REG_R13];
	*--savearea = regs[REG_R12];
	*--savearea = regs[REG_R11];
	*--savearea = regs[REG_R10];
	*--savearea = regs[REG_R9];
	*--savearea = regs[REG_R8];
	*--savearea = regs[REG_RDI];
	*--savearea = regs[REG_RSI];
	*--savearea = regs[REG_RDX];
	*--savearea = regs[REG_RCX];
	*--savearea = regs[REG_RBX];
	*--savearea = regs[REG_RAX];

	regs[REG_RBX] = regs[REG_RDI] = (unsigned long)savearea;
	regs[REG_RSP] = (unsigned long)current->stack + sizeof(current->stack) - 16;
	*(unsigned long *)(regs[REG_RSP] -= sizeof(unsigned long)) = (unsigned long)exittramp;
	regs[REG_RIP] = (unsigned long)bottom;
}

static void alrmtop(int sig, siginfo_t *info, void *ctx) {
	//printf("tick");
	ucontext_t *uc = (ucontext_t *) ctx;
	greg_t *regs = uc->uc_mcontext.gregs;
	hctx_call(regs, timer_bottom);
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

void doswitch(void) {
	struct task *old = current;
	current = runq;
	runq = current->next;

	current_start = sched_gettime();
	ctx_switch(&old->ctx, &current->ctx);

	vmapplymap(&current->vmctx);
}

static void exectramp(void) {
	irq_enable();
	current->main(current->argc, current->argv);
	irq_disable();
	doswitch();
}

int sys_exec(struct hctx *hctx, const char *path, char *const argv[], int argc, main_t func) {
	
	if(func)
	{
		irq_enable();
		unsigned long result = func(argc, argv);
		irq_disable();
		doswitch();
		hctx->rax = result;
		// struct ctx dummy;
		// struct ctx new;
		// ctx_make(&new, exectramp, USERSPACE_START + MAX_USER_MEM - VM_PAGESIZE - 16);

		// irq_disable();
		// current->main = (void *)func;
		// current->argv = argv;
		// current->argc = argc;
		// ctx_switch(&dummy, &new);
		return result;
	}
	char elfpath[32];
	snprintf(elfpath, sizeof(elfpath), "%s.app", path);
	int fd = open(elfpath, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	void *rawelf = mmap(NULL, 128 * 1024, PROT_READ, MAP_PRIVATE, fd, 0);

	if (strncmp(rawelf, "\x7f" "ELF" "\x2", 5)) {
		printf("ELF header mismatch\n");
		return 1;
	}

	// https://linux.die.net/man/5/elf
	//
	// Find Elf64_Ehdr -- at the very start
	//   Elf64_Phdr -- find one with PT_LOAD, load it for execution
	//   Find entry point (e_entry)
	//
	// (we compile loadable apps such way they can be loaded at arbitrary
	// address)

	const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *) rawelf;
	if (!ehdr->e_phoff ||
			!ehdr->e_phnum ||
			!ehdr->e_entry ||
			ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
		printf("bad ehdr\n");
		return 1;
	}
	const Elf64_Phdr *phdrs = (const Elf64_Phdr *) (rawelf + ehdr->e_phoff);

	void *maxaddr = USERSPACE_START;
	for (int i = 0; i < ehdr->e_phnum; ++i) {
		const Elf64_Phdr *ph = phdrs + i;
		if (ph->p_type != PT_LOAD) {
			continue;
		}
		if (ph->p_vaddr < IUSERSPACE_START) {
			printf("bad section\n");
			return 1;
		}
		void *phend = (void*)(ph->p_vaddr + ph->p_memsz);
		if (maxaddr < phend) {
			maxaddr = phend;
		}
	}

	char **copyargv = USERSPACE_START + MAX_USER_MEM - VM_PAGESIZE;
	char *copybuf = (char*)(copyargv + 32);
	char *const *arg = argv;
	char **copyarg = copyargv;
	while (*arg) {
		*copyarg++ = strcpy(copybuf, *arg++);
		copybuf += strlen(copybuf) + 1;
	}
	*copyarg = NULL;

	if (vmbrk(&current->vmctx, maxaddr)) {
		printf("vmbrk fail\n");
		return 1;
	}
	vmmakestack(&current->vmctx);
	vmapplymap(&current->vmctx);

	if (vmprotect(USERSPACE_START, maxaddr - USERSPACE_START, VM_READ | VM_WRITE)) {
		printf("vmprotect RW failed\n");
		return 1;
	}
	for (int i = 0; i < ehdr->e_phnum; ++i) {
		const Elf64_Phdr *ph = phdrs + i;
		if (ph->p_type != PT_LOAD) {
			continue;
		}
		memcpy((void*)ph->p_vaddr, rawelf + ph->p_offset, ph->p_filesz);
		int prot = (ph->p_flags & PF_X ? VM_EXEC  : 0) |
			   (ph->p_flags & PF_W ? VM_WRITE : 0) |
			   (ph->p_flags & PF_R ? VM_READ  : 0);
		if (vmprotect((void*)ph->p_vaddr, ph->p_memsz, prot)) {
			printf("vmprotect section failed\n");
			return 1;
		}
	}

	struct ctx dummy;
	struct ctx new;
	ctx_make(&new, exectramp, (char*)copyargv - 16);

	irq_disable();
	current->main = (void*)ehdr->e_entry;
	current->argv = copyargv;
	current->argc = copyarg - copyargv;
	ctx_switch(&dummy, &new);
}

static void forktramp(void) {
	
	vmapplymap(&current->vmctx);
	
	exittramp();
}

int sys_fork(struct hctx *hctx) {
	struct task *t = &taskpool[taskpool_n++];
	hctx->rax = 0;
	
	vmctx_copy(&t->vmctx, &current->vmctx);
	ctx_make(&t->ctx, forktramp, t->stack + sizeof(t->stack) - 16);
	t->ctx.rbx = hctx;
	policy_run(t);
	
	return t - taskpool;
}

static void timer_bottom(struct hctx *hctx) {
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

static void inittramp2(void) {
	irq_enable();

	init(0, NULL);

	irq_disable();
	doswitch();
}

static void inittramp(void) {
	vmmakestack(&current->vmctx);
	vmapplymap(&current->vmctx);

	struct ctx dummy;
	struct ctx new;
	ctx_make(&new, inittramp2, USERSPACE_START + MAX_USER_MEM - VM_PAGESIZE - 16);
	ctx_switch(&dummy, &new);
}

static void sched_run(int period_ms) {
	sigemptyset(&irqs);
	sigaddset(&irqs, SIGALRM);

	vminit(VM_PAGESIZE * 1024);

	sigset_t none;
	sigemptyset(&none);

	irq_disable();

	{
		struct task *t = &taskpool[taskpool_n++];
		vmctx_make(&t->vmctx);
		ctx_make(&t->ctx, inittramp, t->stack + sizeof(t->stack) - 16);
		policy_run(t);
	}

	tick_period = period_ms;
	timer_init_period(period_ms, alrmtop);

	current = &idle;
	vmctx_make(&current->vmctx);

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

static void segvtop(int sig, siginfo_t *info, void *ctx) {
	ucontext_t *uc = (ucontext_t *) ctx;
	greg_t *regs = uc->uc_mcontext.gregs;

	uint16_t *ins = (uint16_t *)regs[REG_RIP];
	if (*ins != 0x81cd) {
		abort();
	}

	regs[REG_RIP] += 2;

	hctx_call(regs, syscall_bottom);
}

int main(int argc, char *argv[]) {
	struct sigaction act = {
		.sa_sigaction = segvtop,
		.sa_flags = SA_RESTART,
	};
	sigemptyset(&act.sa_mask);

	if (-1 == sigaction(SIGSEGV, &act, NULL)) {
		perror("signal set failed");
		return 1;
	}

	sched_run(100);
	return 0;
}

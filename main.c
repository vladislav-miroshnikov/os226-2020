
#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/ucontext.h>

#include "kernel.h"
#include "sched.h"
#include "syscall.h"
#include "util.h"

extern void init(void);

static void sighnd(int sig, siginfo_t *info, void *ctx) {
	ucontext_t *uc = (ucontext_t *) ctx;
	greg_t *regs = uc->uc_mcontext.gregs;

	uint16_t *ins = (uint16_t *)regs[REG_RIP];
	if (*ins != 0x81cd) {
		abort();
	}

	regs[REG_RIP] += 2;

	unsigned long ret = syscall_do(regs[REG_RAX],
			regs[REG_RBX], regs[REG_RCX],
			regs[REG_RDX], regs[REG_RSI],
			(void *) regs[REG_RDI]);

	regs[REG_RAX] = ret;
}

int main(int argc, char *argv[]) {
	struct sigaction act = {
		.sa_sigaction = sighnd,
		.sa_flags = SA_RESTART,
	};
	sigemptyset(&act.sa_mask);

	if (-1 == sigaction(SIGSEGV, &act, NULL)) {
		perror("signal set failed");
		return 1;
	}

	init();
	sched_run(100);
	return 0;
}

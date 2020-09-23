#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/ucontext.h>
#include "syscall.h"
#include <stdint.h>
#include "util.h"

typedef unsigned long ul;
extern void init(void);
extern int sys_print(char *str, int len);

static void sighnd(int sig, siginfo_t *info, void *ctx) {
	ucontext_t *uc = (ucontext_t *) ctx;
	greg_t *regs = uc->uc_mcontext.gregs;
	uint8_t *ins = (uint8_t *)regs[REG_RIP];
	if (*ins != 0xcd)
	{
		abort();
	}
	if(*(++ins) != 0x81)
	{
		abort();
	}
	//retrieving data from registers
	ul a = uc->uc_mcontext.gregs[REG_RAX];
	const char* word = (const char*) uc->uc_mcontext.gregs[REG_RBX];
	const int len = uc->uc_mcontext.gregs[REG_RCX];
	//register rewriting RAX and instruction pointer offset
	uc->uc_mcontext.gregs[REG_RAX] = sys_print(word, len);
	uc->uc_mcontext.gregs[REG_RIP] += 2;
	
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
	return 0;
}
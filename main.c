
#define _GNU_SOURCE

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <signal.h>
#include <ucontext.h>
#include <sys/ucontext.h>

#define SYSCALL_X(x) \
	x(print, int, 2, char*, argv, int, len) \

#define SC_NR(name, ...) os_syscall_nr_ ## name,
enum syscalls_num {
	SYSCALL_X(SC_NR)
};
#undef SC_NR

static inline long os_syscall(int syscall,
		unsigned long arg1, unsigned long arg2,
		unsigned long arg3, unsigned long arg4,
		void *rest) {
	long ret;
	__asm__ __volatile__(
		"int $0x81\n"
		: "=a"(ret)
		: "a"(syscall), // rax
		  "b"(arg1),    // rbx
		  "c"(arg2),    // rcx
		  "d"(arg3),    // rdx
		  "S"(arg4),    // rsi
		  "D"(rest)     // rdi
		:
	);
	return ret;
}

#define DEFINE0(ret, name) \
	static inline ret os_ ## name (void) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, 0, 0, 0, 0, (void *) 0); \
	}
#define DEFINE1(ret, name, type1, name1) \
	static inline ret os_ ## name (type1 name1) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, (unsigned long) name1, 0, 0, 0, (void *) 0); \
	}
#define DEFINE2(ret, name, type1, name1, type2, name2) \
	static inline ret os_ ## name (type1 name1, type2 name2) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, (unsigned long) name1, (unsigned long) name2, 0, 0, (void *) 0); \
	}
#define DEFINE3(ret, name, type1, name1, type2, name2, type3, name3) \
	static inline ret os_ ## name (type1 name1, type2 name2, type3 name3) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, (unsigned long) name1, (unsigned long) name2, \
				(unsigned long) name3, 0, (void *) 0); \
	}
#define DEFINE4(ret, name, type1, name1, type2, name2, type3, name3, type4, name4) \
	static inline ret os_ ## name (type1 name1, type2 name2, type3 name3, type4 name4) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, (unsigned long) name1, (unsigned long) name2, \
				(unsigned long) name3, (unsigned long) name4, (void *) 0); \
	}
#define DEFINE5(ret, name, type1, name1, type2, name2, type3, name3, type4, name4, type5, name5) \
	static inline ret os_ ## name (type1 name1, type2 name2, type3 name3, type4 name4) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, (unsigned long) name1, (unsigned long) name2, \
				(unsigned long) name3, (unsigned long) name4, (void *) name5); \
	}
#define DEFINE(name, ret, n, ...) \
	DEFINE ## n (ret, name, ## __VA_ARGS__)
SYSCALL_X(DEFINE)
#undef DEFINE0
#undef DEFINE1
#undef DEFINE2
#undef DEFINE3
#undef DEFINE4
#undef DEFINE5
#undef DEFINE

static int g_retcode;

int sys_print(char *str, int len) {
	return write(1, str, len);
}

int os_printf(const char *fmt, ...) {
	char buf[128];
	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return os_print(buf, ret);
}

int echo(int argc, char *argv[]) {
	for (int i = 1; i < argc; ++i) {
		os_printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
	}
	return argc - 1;
}

int retcode(int argc, char *argv[]) {
	os_printf("%d\n", g_retcode);
	return 0;
}

int readmem(int argc, char *argv[]) {
	unsigned long ptr;
	if (1 != sscanf(argv[1], "%lu", &ptr)) {
		os_printf("cannot parse address\n");
		return 1;
	}

	os_printf("%d\n", *(int*)ptr);
	return 0;
}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#define APPS_X(X) \
	X(echo) \
	X(retcode) \
	X(readmem) \

#define DECLARE(X) int X(int, char *[]);
APPS_X(DECLARE)
#undef DECLARE

static const struct app {
	const char *name;
	int (*fn)(int, char *[]);
} app_list[] = {
#define ELEM(X) { # X, X },
	APPS_X(ELEM) 
#undef ELEM
};

int exec(int argc, char *argv[]) {
	const struct app *app = NULL;
	for (int i = 0; i < ARRAY_SIZE(app_list); ++i) {
		if (!strcmp(argv[0], app_list[i].name)) {
			app = &app_list[i];
			break;
		}
	}

	if (!app) {
		os_printf("Unknown command\n");
		return 1;
	}

	g_retcode = app->fn(argc, argv);
	return g_retcode;
}

int shell(int argc, char *argv[]) {
	char line[256];
	while (fgets(line, sizeof(line), stdin)) {
		const char *comsep = "\n;";
		char *stcmd;
		char *cmd = strtok_r(line, comsep, &stcmd);
		while (cmd) {
			const char *argsep = " ";
			char *starg;
			char *arg = strtok_r(cmd, argsep, &starg);
			char *argv[256];
			int argc = 0;
			while (arg) {
				argv[argc++] = arg;
				arg = strtok_r(NULL, argsep, &starg);
			}
			argv[argc] = NULL;

			if (!argc) {
				break;
			}

			exec(argc, argv);

			cmd = strtok_r(NULL, comsep, &stcmd);
		}
	}
	return 0;
}

void sighnd(int sig, siginfo_t *info, void *ctx) {
	ucontext_t *uc = (ucontext_t *) ctx;
	greg_t *regs = uc->uc_mcontext.gregs;
}

int main(int argc, char *argv[]) {
	struct sigaction act = {
		.sa_sigaction = sighnd,
		.sa_flags = SA_RESTART,
	};
	sigemptyset(&act.sa_mask);

	if (-1 == sigaction(SIGSEGV, &act, NULL)) {
		perror("signal set failed");
		exit(1);
	}
	return shell(0, NULL);
}

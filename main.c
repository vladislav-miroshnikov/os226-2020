
#define _GNU_SOURCE

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <signal.h>
#include <ucontext.h>
#include <sys/ucontext.h>

static int g_retcode;

int echo(int argc, char *argv[]) {
	for (int i = 1; i < argc; ++i) {
		printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
	}
	return argc - 1;
}

int retcode(int argc, char *argv[]) {
	printf("%d\n", g_retcode);
	return 0;
}

int readmem(int argc, char *argv[]) {
	unsigned long ptr;
	if (1 != sscanf(argv[1], "%lu", &ptr)) {
		printf("cannot parse address\n");
		return 1;
	}

	printf("%d\n", *(int*)ptr);
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
		printf("Unknown command\n");
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

unsigned f(unsigned val, int h, int l) {
        return (val & ((1ul << (h + 1)) - 1)) >> l;
}       
 
int enc2reg(unsigned enc) { 
        switch(enc) {
        case 0: return REG_RAX;
        case 1: return REG_RCX;
        case 2: return REG_RDX;
        case 3: return REG_RBX;
        case 4: return REG_RSP;
        case 5: return REG_RBP;
        case 6: return REG_RSI;
        case 7: return REG_RDI;
        default: break;
        }
        abort();
}

void sighnd(int sig, siginfo_t *info, void *ctx) {
        ucontext_t *uc = (ucontext_t *) ctx;
        greg_t *regs = uc->uc_mcontext.gregs;

	static int n_calls;

        uint8_t *ins = (uint8_t *)regs[REG_RIP];
        if (ins[0] != 0x8b) {
                abort();
        }

        uint8_t *next = &ins[2];

        int dst = enc2reg(f(ins[1], 5, 3));

        int rm = f(ins[1], 3, 0);
        if (rm == 4) {
                abort();
        }
        int base = enc2reg(rm);

        int off = 0;
        switch(f(ins[1], 7, 6)) {
        case 0:
                break;
        case 1:
                off = *(int8_t*)next;
                next += 1;
                break;
        case 2:
                off = *(uint32_t *)&next;
                next += 4;
                break;
        default:
                break;
        }

        regs[dst] = 100 + (++n_calls);
        regs[REG_RIP] = (unsigned long)next;
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

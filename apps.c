#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <time.h>
#include <sys/time.h>

#include <unistd.h>
#include <fcntl.h>
#include <elf.h>
#include <sys/mman.h>

#include "sched.h"
#include "vm.h"
#include "syscall.h"
#include "util.h"
#include "libc.h"

#define APPS_X(X) \
	X(echo) \
	X(retcode) \
	X(readmem) \
	X(sysecho) \
	X(sleep) \
	X(burn) \
	X(load) \
	X(loadproc) \

#define DECLARE(X) static int app_ ## X(int, char *[]);
APPS_X(DECLARE)
#undef DECLARE

static int g_retcode;
static long refstart;

static const struct app {
	const char *name;
	int (*fn)(int, char *[]);
} app_list[] = {
#define ELEM(X) { # X, app_ ## X },
	APPS_X(ELEM)
#undef ELEM
};

struct execargs {
	int argc;
	char* argv[16];
	char argvbuf[256];
};

static int exec(int argc, char *argv[]) {
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

static int app_echo(int argc, char *argv[]) {
	for (int i = 1; i < argc; ++i) {
		printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
	}
	return argc - 1;
}

static int app_retcode(int argc, char *argv[]) {
	printf("%d\n", g_retcode);
	return 0;
}

static int app_readmem(int argc, char *argv[]) {
	unsigned long ptr;
	if (1 != sscanf(argv[1], "%lu", &ptr)) {
		printf("cannot parse address\n");
		return 1;
	}

	printf("%d\n", *(int*)ptr);
	return 0;
}

static int app_sysecho(int argc, char *argv[]) {
	for (int i = 1; i < argc - 1; ++i) {
		os_print(argv[i], strlen(argv[i]));
		os_print(" ", 1);
	}
	if (1 < argc) {
		os_print(argv[argc - 1], strlen(argv[argc - 1]));
		os_print("\n", 1);
	}
	return argc - 1;
}


struct app_ctx {
	int param;
};
struct app_ctx app_ctxs[16];

static long refstart;
static long reftime(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
static void print(struct app_ctx *ctx, const char *msg) {
	printf("app1 id %d %s time %d reference %ld\n",
			ctx - app_ctxs, msg, sched_gettime(), reftime() - refstart);
	fflush(stdout);
}

static void sleep_entry(void *_ctx) {
	struct app_ctx *ctx = _ctx;
	while (1)  {
		print(ctx, "sleep");
		sched_sleep(ctx->param);
	}
}

static void burn_entry(void *_ctx) {
	struct app_ctx *ctx = _ctx;
	while (1)  {
		print(ctx, "burn");
		for (volatile int i = 100000 * ctx->param; 0 < i; --i) {
		}
	}
}

static int app_burn(int argc, char* argv[]) {
	int id = atoi(argv[1]);
	struct app_ctx *ctx = &app_ctxs[id];

	ctx->param = atoi(argv[2]);
	void (*entry)(void*) = !strcmp(argv[0], "burn") ?
		burn_entry : sleep_entry;

	sched_new(entry, ctx, 0);
}

static int app_sleep(int argc, char* argv[]) {
	app_burn(argc, argv);
}

static int app_cosched(int argc, char* argv[]) {
	sched_run(100);
}

static int app_load(int argc, char* argv[]) {
	char path[32];
	snprintf(path, sizeof(path), "%s.app", argv[1]);
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	static char rawelf[128 * 1024];

	int bytes;
	char *p = rawelf;
	do {
		bytes = read(fd, p, rawelf + sizeof(rawelf) - p);
		if (bytes < 0) {
			perror("read");
			return 1;
		}
		p += bytes;
	} while (0 < bytes);
	if (p == rawelf + sizeof(rawelf)) {
		printf("unsufficient space for raw elf\n");
		return 1;
	}

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

	unsigned long maxaddr = IUSERSPACE_START;
	for (int i = 0; i < ehdr->e_phnum; ++i) {
		const Elf64_Phdr *ph = phdrs + i;
		if (ph->p_type != PT_LOAD) {
			continue;
		}
		if (ph->p_vaddr < IUSERSPACE_START) {
			printf("bad section\n");
			return 1;
		}
		unsigned phend = ph->p_vaddr + ph->p_memsz;
		if (maxaddr < phend) {
			maxaddr = phend;
		}
	}

	if (vmbrk((void*)maxaddr)) {
		printf("vmbrk fail\n");
		return 1;
	}
	if (vmprotect(USERSPACE_START, maxaddr - IUSERSPACE_START, VM_READ | VM_WRITE)) {
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

	return ((int(*)(int, char**))ehdr->e_entry)(argc - 1, argv + 1);
}

static void loadproc(void *args) {
	struct execargs *eas = args;
	app_load(eas->argc, eas->argv);
}

static int app_loadproc(int argc, char* argv[]) {
	static struct execargs execargs[16];
	static int execargsn;

	struct execargs *eas = &execargs[execargsn++];
	eas->argc = argc;
	char *bp = eas->argvbuf;
	for (int i = 0; i < argc; ++i) {
		eas->argv[i] = strcpy(bp, argv[i]);
		bp += strlen(bp) + 1;
	}

	sched_new(loadproc, eas, 0);
	return 0;
}

static void shell(void *ctx) {
	char line[256];
	while (fgets(line, sizeof(line), stdin)) {
		const char *comsep = "\n;";
		char *stcmd;
		char *cmd = strtok_r(line, comsep, &stcmd);
		while (cmd) {
			const char *argsep = " \t";
			char *starg;
			char *arg = strtok_r(cmd, argsep, &starg);

			if (arg[0] == '#') {
				break;
			}

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
}

void init(void) {
	refstart = reftime();
	sched_new(shell, NULL, 0);
}

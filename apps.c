
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <time.h>
#include <sys/time.h>

#include "sched.h"
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

#define DECLARE(X) static int X(int, char *[]);
APPS_X(DECLARE)
#undef DECLARE

static int g_retcode;

static const struct app {
	const char *name;
	int (*fn)(int, char *[]);
} app_list[] = {
#define ELEM(X) { # X, X },
	APPS_X(ELEM)
#undef ELEM
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

static int echo(int argc, char *argv[]) {
	for (int i = 1; i < argc; ++i) {
		printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
	}
	return argc - 1;
}

static int retcode(int argc, char *argv[]) {
	printf("%d\n", g_retcode);
	return 0;
}

static int readmem(int argc, char *argv[]) {
	unsigned long ptr;
	if (1 != sscanf(argv[1], "%lu", &ptr)) {
		printf("cannot parse address\n");
		return 1;
	}

	printf("%d\n", *(int*)ptr);
	return 0;
}

static int sysecho(int argc, char *argv[]) {
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

static int burn(int argc, char* argv[]) {
	int id = atoi(argv[1]);
        struct app_ctx *ctx = &app_ctxs[id];

	ctx->param = atoi(argv[2]);
	void (*entry)(void*) = !strcmp(argv[0], "burn") ?
		burn_entry : sleep_entry;

	sched_new(entry, ctx, 0);
}

static int sleep(int argc, char* argv[]) {
	burn(argc, argv);
}

static int cosched(int argc, char* argv[]) {
	sched_run(100);
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "sched.h"
#include "syscall.h"
#include "util.h"
#include "libc.h"

#define APPS_X(X) \
	X(echo) \
	X(retcode) \
	X(readmem) \
	X(sysecho) \
	X(cosched_policy) \
	X(coapp) \
	X(cosched) \

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

struct coapp_ctx {
        int cnt;
};
struct coapp_ctx app_ctxs[16];

static void coapp_task(void *_ctx) {
	struct coapp_ctx *ctx = _ctx;

        printf("%16s id %d cnt %d\n", __func__, ctx - app_ctxs, ctx->cnt);

        if (0 < ctx->cnt) {
                sched_cont(coapp_task, ctx, 2);
        }

        --ctx->cnt;
}

static void coapp_rt(void *_ctx) {
	struct coapp_ctx *ctx = _ctx;

        printf("%16s id %d cnt %d\n", __func__, ctx - app_ctxs, ctx->cnt);

        sched_time_elapsed(1);

        if (0 < ctx->cnt) {
                sched_cont(coapp_rt, ctx, 0);
        }

        --ctx->cnt;
}

static int cosched_policy(int argc, char* argv[]) {
	sched_set_policy(atoi(argv[1]));
}

static int coapp(int argc, char* argv[]) {
	int id = atoi(argv[1]);
	int entry_id = atoi(argv[2]) - 1;

        struct coapp_ctx *ctx = &app_ctxs[id];
	ctx->cnt = atoi(argv[3]);

	void (*entries[])(void*) = { coapp_task, coapp_rt };
	sched_new(entries[entry_id], ctx, atoi(argv[4]), atoi(argv[5]), entry_id);
}

static int cosched(int argc, char* argv[]) {
	sched_run();
}

static int shell(int argc, char *argv[]) {
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
	return 0;
}

void init(void) {
	char *argv[] = { "shell", NULL };
	shell(1, argv);
}
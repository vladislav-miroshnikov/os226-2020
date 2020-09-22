<<<<<<< HEAD
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
=======

#define _GNU_SOURCE

>>>>>>> 357451a0ec4b55ab76958efba85aff811d56b44e
#include <stdio.h>
#include <signal.h>
#include <ucontext.h>
#include <sys/ucontext.h>

<<<<<<< HEAD
int code = 0;

int echo(int argc, char *argv[]) 
{
	for (int i = 1; i < argc; ++i)
    {

		printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
	}
	return argc - 1;
}


int retcode(int argc, char *argv[]) 
{
	printf("%d\n", code);
}

void parser(char line[], char* firstSubstring, char* secondSubstring, int argc, char *argv[])
{
	char* ptr = strtok_r(line, ";\n", &firstSubstring);

		while(ptr != NULL)
		{
			char* str = strtok_r(ptr, " ", &secondSubstring);
			argc = 0;

			while(str != NULL)
			{
				argv[argc] = str;
				str = strtok_r(NULL, " ", &secondSubstring);
				argc++;
			}

			if(!strcmp(argv[0], "echo"))
			{
				code = echo(argc, argv);
			}

			if(!strcmp(argv[0], "retcode"))
			{
				retcode(argc, argv);
			}			
			
			ptr = strtok_r(NULL, ";\n", &firstSubstring);
			
			
		}	
=======
#include "syscall.h"
#include "util.h"

extern void init(void);

static void sighnd(int sig, siginfo_t *info, void *ctx) {
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
		return 1;
	}

	init();
	return 0;
>>>>>>> 357451a0ec4b55ab76958efba85aff811d56b44e
}

int main(int argc, char *argv[]) 
{
	FILE *file = stdin;
	char line[512];
	char* firstSubstring = NULL;
	char* secondSubstring = NULL;

	while(fgets ( line, sizeof line, file ) != NULL )
	{
		parser(line, firstSubstring, secondSubstring, argc, argv);
		
	}
    
    return 0;
}
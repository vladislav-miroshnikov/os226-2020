#define _GNU_SOURCE

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "timer.h"

static struct timeval gen_t; //not to touch initv

int timer_cnt(void) {
	struct itimerval timer_val_rest;
	getitimer(ITIMER_REAL, &timer_val_rest); //see how much time in timer left
	//cast to ms
	int delta1 = (gen_t.tv_sec - timer_val_rest.it_value.tv_sec)*1000;
	int delta2 = (gen_t.tv_usec - timer_val_rest.it_value.tv_usec)/1000;
	return delta1 + delta2; //get how much time has passed since the last trigger in ms
}

extern void timer_init_period(int ms, hnd_t hnd) {
	struct timeval initv = {  //first initialization
		.tv_sec  = ms / 1000,
		.tv_usec = ms * 1000,
	};
	gen_t = initv;
	const struct itimerval setup_it = {
		.it_value    = initv,
		.it_interval = initv,
	};

	if (-1 == setitimer(ITIMER_REAL, &setup_it, NULL)) {
		perror("setitimer");
	}

	struct sigaction act = {
		.sa_sigaction = hnd,
		.sa_flags = SA_RESTART,
	};
	sigemptyset(&act.sa_mask);

	if (-1 == sigaction(SIGALRM, &act, NULL)) {
		perror("signal set failed");
		exit(1);
	}
}



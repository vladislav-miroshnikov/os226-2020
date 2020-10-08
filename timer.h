#pragma once

#include <signal.h>

extern int timer_cnt(void);

typedef void hnd_t(int sig, siginfo_t *info, void *ctx);
extern void timer_init_period(int ms, hnd_t hnd);



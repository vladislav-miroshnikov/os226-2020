#pragma once
#include "vm.h"

#include "ctx.h"
#include "vm.h"

enum policy {
	// first-in, first-out; run tasks in order of their arrival
	POLICY_FIFO,

	// highest priority task (highest priority value) should be executed
	// first. Use round-robin for processes with same priority
	// (task from 1st process, from 2nd, ... Nth, 1st, 2nd, ...)
	POLICY_PRIO,

	// consider deadline, execute process with Earliest Deadline First.
	// Fallback to priority policy if deadlines are equal
	POLICY_DEADLINE,
};

// Set policy for future tasks
extern void sched_set_policy(enum policy policy);

// Add new task
extern void sched_new(void (*entrypoint)(void *ctx), // entrypoint function
		void *ctx,     // context of the process
		int priority);  // priority, [0 - 10], bigger for more priority

// Continue process from function after some amount of time
extern void sched_cont(void (*entrypoint)(void *aspace), // entrypoint function
		void *aspace,// addresses the process can access
		int timeout); // when the continuation became runnable

// Notify scheduler that some amount of time passed
extern void sched_sleep(unsigned amount);

extern int sched_gettime(void);

extern void doswitch();
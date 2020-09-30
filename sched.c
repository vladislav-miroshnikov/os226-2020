#include "sched.h"
#include <stdio.h>

struct task {
	void (*entry)(void *ctx);
	void *ctx;
	int priority;
	int deadline;
	int id;
};

static struct task taskpool[16];
static int taskpool_n;
static int priorities[11]; //counter-array of priority
static enum policy policy;


void sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority,
		int deadline) {
	struct task *t = &taskpool[taskpool_n++];
	t->entry = entrypoint;
	t->ctx = aspace;
	t->priority = priority;
	t->deadline = deadline;
	t->id = taskpool_n;
	priorities[priority] += 1;
	t->deadline = (deadline > 0) ? deadline : 0;
}

void sched_cont(void (*entrypoint)(void *aspace),
		void *aspace,
		int timeout) {

	
}

void sched_time_elapsed(unsigned amount) {
	// ...
}

void sched_set_policy(enum policy _policy) {
	policy = _policy;

}

void sched_run(void) {

	switch(policy)
	{
		case 0:
		exec_fifo_policy();
		break;

		case 1:
		exec_prior_policy();
		break;

		case 2:
		exec_deadline_policy();
		break;

		default:
		perror("Incorrect policy");
	}

	
}

int priority_compare(const void *t1, const void *t2)
{
	const struct task *a = t1, *b = t2; 
	return (b->priority - a->priority);
}

int id_compare(const void *t1, const void *t2)
{
	const struct task *a = t1, *b = t2; 
	return (a->id - b->id);
}

int deadline_compare(const void *t1, const void *t2)
{
	const struct task *a = t1, *b = t2; 
	return (b->deadline - a->deadline);
}

void exec_fifo_policy(void)  //rewrite with array shift and round-rb
{
	while (taskpool_n != 0)
	{
		for(int i = 0; i < taskpool_n; i++)
		{
			if (*((int*)taskpool[i].ctx) >= 0)
			{
				taskpool[i].entry(taskpool[i].ctx);
			}
			else
			{
				for(int j = i; j < taskpool_n; j++)
				{
					struct task tmp = taskpool[j];
					taskpool[j] = taskpool[j+1];
					taskpool[j+1] = tmp;
				}			
				taskpool_n--;				
			}		
		}
	}
}


void exec_prior_policy(void)
{
	qsort(taskpool, taskpool_n, sizeof(struct task), priority_compare);
	
	while(taskpool_n != 0)
	{
		deadline_priority();
	}
}

void deadline_priority(void)
{
	
	for(int i = 10 ; i >= 0; i--)
	{
		if(priorities[i] == 0)
		{
			continue;
		}
		else if(priorities[i] == 1)
		{
			int ctn = *((int*)taskpool[0].ctx);
			for (int j = 0; j <= ctn; j++)
			{
				taskpool[0].entry(taskpool[0].ctx);
			}
			array_shift(0);
			taskpool_n--;
		}
		else
		{
			qsort(taskpool, priorities[i], sizeof(struct task), id_compare);
			round_robin(priorities[i]);
			taskpool_n -= priorities[i];
		}
	}
}

void round_robin(int count)
{
	while (count != 0)
	{
		for(int i = 0; i < count; i++)
		{
			if (*((int*)taskpool[i].ctx) >= 0)
			{
				taskpool[i].entry(taskpool[i].ctx);
			}
			else
			{
				round_robin(i);			
				count--;				
			}		
		}
	}
}

void array_shift(int i)
{
	for(int j = i; j < taskpool_n; j++)
	{
		struct task tmp = taskpool[j];
		taskpool[j] = taskpool[j+1];
		taskpool[j+1] = tmp;
	}	
}

void exec_deadline_policy(void)
{
	qsort(taskpool, taskpool_n, sizeof(struct task), deadline_compare);
	while(taskpool[0].deadline == 0)
	{
		array_shift(0);
	}
	while(taskpool_n != 0)
	{	
		for(int i = 0; i < taskpool_n; i++)
		{
			int j = 0;
			while((taskpool[j].deadline == taskpool[j+1].deadline) && (j < taskpool_n - 1))
			{
				j++;
			}
			j++;
			if (j == 1)
			{
				int ctn = *((int*)taskpool[0].ctx);
				for (int a = 0; a <= ctn; a++)
				{
					taskpool[0].entry(taskpool[0].ctx);
				}
				array_shift(0);
				priorities[taskpool[0].priority] -= 1;
				taskpool_n--;
			}
			else
			{

				deadline_priority();
			}

		}
	}
}
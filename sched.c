#include "sched.h"
#include <stdio.h>

#define DEFINEPOLICY(X)\
return exec_##X##_policy();
#define DEFINE0()\
DEFINEPOLICY(fifo)
#define DEFINE1()\
DEFINEPOLICY(prior)
#define DEFINE2()\
DEFINEPOLICY(deadline)
#define DEFINE(n)\
DEFINE##n()


struct task {
	void (*entry)(void *ctx);
	void *ctx;
	int priority;
	int deadline;
	int id;
	int timer;
	int type; //i use a variable to idetenficate : 0-coapp_task, 1-coapp_rt, and  kernel sched.c does not knew anything about application apps.c, 
	//these are just formal values
};

static struct task taskpool[16];
static int taskpool_n;
static int priorities[11]; //counter-array of priority
static enum policy policy;
static void array_shift(int i);
static void exec_fifo_policy(void);
static void exec_prior_policy(void);
static void exec_deadline_policy(void);
static void round_robin(int count);
static void deadline_priority(void);
static void timer_work(int i);
int qsort();

void sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority,
		int deadline, int type) {
	struct task *t = &taskpool[taskpool_n++];
	t->entry = entrypoint;
	t->ctx = aspace;
	t->priority = priority;
	t->deadline = deadline;
	t->id = taskpool_n;
	priorities[priority] += 1;
	t->deadline = (deadline > 0) ? deadline : 0;
	t->timer = 0;
	t->type = type;
}

//!!!change the timer value of the task
void sched_cont(void (*entrypoint)(void *aspace),
		void *aspace,
		int timeout) {
			for(int i = 0; i < taskpool_n; i++)
			{
				if(taskpool[i].ctx == aspace)
				{
					taskpool[i].timer = timeout;
				}
			}
	
}

//!!! reduces the timer value by one
void sched_time_elapsed(unsigned amount) {
	for(int i = 0; i < taskpool_n; i++)
	{
		if(taskpool[i].timer > 0)
		{
			taskpool[i].timer -= (int) amount;
		}
	}
}

void sched_set_policy(enum policy _policy) {
	policy = _policy;

}

 
void sched_run(void) {

	switch(policy)
	{
		case 0:
		DEFINE(0);
		break;

		case 1:
		DEFINE(1);
		break;

		case 2:
		DEFINE(2);
		break;

		default:
		perror("Incorrect policy");
	}

}

//region comparators

static int priority_compare(const void *t1, const void *t2)
{
	const struct task *a = t1, *b = t2; 
	return (b->priority - a->priority);
}

static int id_compare(const void *t1, const void *t2)
{
	const struct task *a = t1, *b = t2; 
	return (a->id - b->id);
}

static int deadline_compare(const void *t1, const void *t2)
{
	const struct task *a = t1, *b = t2; 
	return (b->deadline - a->deadline);
}

//endregion


static void exec_fifo_policy(void)  
{
	while (taskpool_n != 0)
	{
		round_robin(taskpool_n);
		taskpool_n = 0;
	}
}


static void exec_prior_policy(void)
{
	qsort(taskpool, taskpool_n, sizeof(struct task), priority_compare);
	
	while(taskpool_n != 0)
	{
		deadline_priority();
	}
}

static void exec_deadline_policy(void)
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
			while((taskpool[j].deadline == taskpool[j+1].deadline) && (j < taskpool_n - 1))  //find tasks with same deadlines
			{
				j++;
			}
			j++;
			if (j == 1)
			{
				timer_work(i);						
			}
			else
			{
				deadline_priority();
			}

		}
	}
}

static void round_robin(int count) //general part for FIFO, Priority or Deadline policy execution of tasks without regard priorities or deadlines
{
	int rt_count = 0;
	for(int i = 0; i < count; i++)
	{
		if(taskpool[i].type == 1)
		{
			rt_count++;
		}
	}
	while (count != 0)
	{
		for(int i = 0; i < count; i++)
		{
			if(taskpool[i].timer == 0)  //execute tasks coapp_tasks and coapp_rt with timer = 0
			{
				if (*((int*)taskpool[i].ctx) >= 0)
				{
					taskpool[i].entry(taskpool[i].ctx);
				}
				else
				{
					if(taskpool[i].type == 1)
					{
						rt_count--;
					}
					array_shift(i);			
					count--;		
							
				}	
			}	
			else if(rt_count == 0) //if there are no coapp_rt tasks left, we complete all coapp_task tasks
			{
				
				if (*((int*)taskpool[i].ctx) >= 0)
				{
					taskpool[i].entry(taskpool[i].ctx);
				}
				else
				{
					array_shift(i);			
					count--;				
				}
			}
			
		}
	}
}

static void deadline_priority(void) //general part for Priority or Deadline policy
{
	while(taskpool_n != 0)
	{
		for(int i = 10 ; i >= 0; i--)
		{
			if(priorities[i] == 0)
			{
				continue;
			}
			else if(priorities[i] == 1)
			{
				timer_work(i);
			}
			else
			{
				qsort(taskpool, priorities[i], sizeof(struct task), id_compare);
				round_robin(priorities[i]);
				taskpool_n -= priorities[i];
			}
		}
	}
	
}

static void timer_work(int i)
{
	if(taskpool[0].timer == 0)
	{
					
		if(taskpool[0].type == 1) //a case when task is coapp_rt and so we make it all
		{
			int ctn = *((int*)taskpool[0].ctx);
			for (int j = 0; j <= ctn; j++)
			{
				taskpool[0].entry(taskpool[0].ctx);
			}
			array_shift(0);
			taskpool_n--;
			priorities[i] -= 1;
		}
		else //a case when task is coapp_task and we make one iteration
		{
			if (*((int*)taskpool[0].ctx) >= 0)
			{
				taskpool[0].entry(taskpool[0].ctx);
			}
			else
			{
				array_shift(0);
				taskpool_n--;
				priorities[i] -= 1;		
			}
							
			return;
		}
					
	}
	else // the case when the first task timer != 0, then we look for the first task with timer = 0 and execute it
	{
		for(int i = 1; i < taskpool_n;i++)
		{
			int rt_count = 0;
			if(taskpool[i].timer == 0 && taskpool[i].type == 1)
			{
				rt_count++;
				if (*((int*)taskpool[i].ctx) >= 0)
				{
					taskpool[i].entry(taskpool[i].ctx);
				}
				else
				{
					array_shift(i);
					taskpool_n -= 1;
				}
				
				break;
			}
			if(rt_count == 0) //if there are no coapp_rt tasks left, we complete all coapp_task tasks
			{
				
				while(taskpool_n != 0)
				{
					for(int i = 0; i < taskpool_n; i++)
					{
						if (*((int*)taskpool[i].ctx) >= 0)
						{
							taskpool[i].entry(taskpool[i].ctx);
						}
						else
						{
							array_shift(i);			
							taskpool_n--;				
						}	
					}
				}
				
			}
		}
		return;
	}		
}

static void array_shift(int i) //auxiliary function for array shift
{
	for(int j = i; j < taskpool_n; j++)
	{
		struct task tmp = taskpool[j];
		taskpool[j] = taskpool[j+1];
		taskpool[j+1] = tmp;
	}	
}


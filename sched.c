#include "sched.h"
#include <stdio.h>

struct task {
	void (*entry)(void *ctx);
	void *ctx;
	int priority;
	int deadline;
	int id;
	int out_time;
	struct task *next;
	struct task *next_wait;
};

static struct task taskpool[16];
static int taskpool_n;
static int time;
static int(*policy_cmp)(struct task *t1, struct task *t2); //func-comparator pointer
static struct task *running_list;
static struct task *waiting_list;
static void smart_sort();
static void array_shift(int i);
static int fifo_cmp(struct task *t1, struct task *t2);
static int prio_cmp(struct task *t1, struct task *t2);
static int deadline_cmp(struct task *t1, struct task *t2);
int qsort();

void sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority,
		int deadline) {
	struct task *t = &taskpool[taskpool_n++];
	t->entry = entrypoint;
	t->ctx = aspace;
	t->priority = priority;
	t->deadline = deadline > 0 ? deadline : 0;
	t->id = t - taskpool;
}

void fifo_insert(struct task *tmp, struct task *t)
{
	if(policy_cmp == deadline_cmp && (tmp->deadline == 0 || t->deadline == 0))
	{
		if(t->deadline == 0 && tmp->deadline != 0)
		{
			struct task *tmp2 = running_list;
			while(tmp2)
			{
				if(!tmp2->next)
				{
					break;
				}

				tmp2 = tmp2->next;
				
			}
			tmp2->next = t;
			t->next = NULL;
		}
		else if(t->deadline != 0 && tmp->deadline == 0)
		{
			t->next = tmp;
			running_list = t;
		}
		
	}
	else
	{
		while(tmp && policy_cmp(tmp, t) < 0)
		{
			if(!tmp->next)
			{
				break;
			}
			tmp = tmp->next;
		}
	}
	
	struct task *tmp_prev = running_list;
	if(tmp_prev != tmp)
	{
		while(tmp_prev->next)
		{
			if(tmp_prev->next == tmp)
			{
				break;
			}
			tmp_prev = tmp_prev->next;
		}
		tmp_prev->next = t;
		t->next = tmp;
	}
	else
	{
		if(policy_cmp == fifo_cmp)
		{
			struct task *ut = tmp_prev->next;
			tmp_prev->next = t;
			t->next = ut;
		}
		else if(policy_cmp == prio_cmp)
		{
			if (t->priority > tmp->priority)
			{
				t->next = tmp; 
				running_list = t;
			}
			else if(t->priority < tmp->priority)
			{
				tmp->next = t;
				t->next = NULL;
			}			
		}
	}
	
}

void prior_insert(struct task *tmp, struct task *t)
{
	if((tmp->priority - t->priority) == 0)
	{
		policy_cmp = fifo_cmp;
		fifo_insert(tmp, t);
	}
	else
	{
		fifo_insert(tmp,t);
	}
}

void smart_insert(struct task *t)
{
	if(running_list)
	{
		struct task *tmp = running_list;
		if(policy_cmp == fifo_cmp)
		{
			fifo_insert(tmp, t);
		}
		else if(policy_cmp == prio_cmp)
		{
			prior_insert(tmp,t);
		}
		else if(policy_cmp == deadline_cmp)
		{
			if((tmp->deadline - t->deadline) == 0)
			{
				policy_cmp = prio_cmp;
				prior_insert(tmp, t);
			}
			else
			{
				fifo_insert(tmp, t);				
			}
		
		}
	}
	else
	{
		running_list = t;
	}	

}

void sched_cont(void (*entrypoint)(void *aspace),
		void *aspace,
		int timeout) {
		
		
	if (timeout)
	{
		for(int i = 0; i < taskpool_n; i++)
		{
			if (taskpool[i].ctx == aspace)
			{
				
				taskpool[i].out_time = time + timeout;
				
				if(!waiting_list)
				{
					waiting_list = &taskpool[i];
					waiting_list->next = NULL;
				}
				else
				{
					struct task* tmp = &taskpool[i];
					tmp->next = NULL;
					struct task* tmp2 = waiting_list;

					while(1)
					{
						if(!tmp2->next_wait)
						{
							break;
						}

						tmp2 = tmp2->next_wait;
					}

					tmp2->next_wait = tmp;
				}
				
			}
		}
	}
	else
	{
		for(int i = 0; i < taskpool_n; i++)
		{
			if (taskpool[i].ctx == aspace)
			{
				struct task *t = &taskpool[i];
				smart_insert(t);
				break;
			}
		}
	}
	
}

void sched_time_elapsed(unsigned amount) {
	time += (int)amount;
	if(waiting_list)
	 {
	 	struct task* tmp = waiting_list;
	 	while(1)
	 	{
	 		if(tmp->out_time <= time)
	 		{
	 			smart_insert(tmp);

	 			struct task *tmp_prev = waiting_list;
	 			 while(tmp_prev->next_wait)
	 			 {
					if (tmp_prev->next_wait == tmp)
					{
						break;
					}
	 			 	tmp_prev = tmp_prev->next_wait;
	 			 }
				 if (tmp == tmp_prev)
				 {
					waiting_list = NULL;
				 }
				 else
				 {
					tmp_prev->next_wait = tmp->next_wait;
				 }
	 		}

	 		if(!tmp->next_wait)
	 		{
	 			break;
	 		}

	 		tmp = tmp->next_wait;
	 	}

	 }
}


void sched_set_policy(enum policy policy) {
	switch (policy)
	{
	case POLICY_FIFO:
		policy_cmp = fifo_cmp;
		break;
	
	case POLICY_PRIO:
		policy_cmp = prio_cmp;
		break;

	case POLICY_DEADLINE:
		policy_cmp = deadline_cmp;
		break;

	default:
		perror("Incorrect policy");
	}
}

void sched_run(void) {
	smart_sort();
	for(int i = 0; i < taskpool_n - 1; i++)
	{
		taskpool[i].next = &taskpool[i+1];
	}

	running_list = &taskpool[0];
	struct task *tmp;
 	do
	{
		tmp = running_list;
		running_list = running_list->next;
		tmp->entry(tmp->ctx);

	} while (running_list);
	
}

static void smart_sort()
{
	qsort(taskpool, taskpool_n, sizeof(struct task), policy_cmp);
	if(policy_cmp == deadline_cmp)
	{
		while(taskpool[0].deadline == 0)
		{
			array_shift(0);
		}
	}
}

static void array_shift(int i) //auxiliary function for array shift
{
	for(int j = i; j < taskpool_n - 1; j++)
	{
		struct task tmp = taskpool[j];
		taskpool[j] = taskpool[j+1];
		taskpool[j+1] = tmp;
	}	
}

static int fifo_cmp(struct task *t1, struct task *t2)
{
	return t1->id - t2->id; 
}

static int prio_cmp(struct task *t1, struct task *t2)
{
	int delta = t2->priority - t1->priority;
	if(delta)
	{
		return delta;
	}
	return fifo_cmp(t1, t2);
}


static int deadline_cmp(struct task *t1, struct task *t2)
{
	int delta = t1->deadline - t2->deadline;
	if(delta)
	{
		return delta;
	}
	return prio_cmp(t1, t2);
}



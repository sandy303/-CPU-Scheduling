#include <inc/assert.h>

#include <kern/sched.h>
#include <kern/user_environment.h>
#include <kern/memory_manager.h>
#include <kern/command_prompt.h>
#include <kern/trap.h>
#include <kern/kheap.h>
#include <kern/utilities.h>

//void on_clock_update_WS_time_stamps();
extern uint32 isBufferingEnabled();
extern void cleanup_buffers(struct Env* e);
extern inline uint32 pd_is_table_used(struct Env *e, uint32 virtual_address);
extern inline void pd_set_table_unused(struct Env *e, uint32 virtual_address);
extern inline void pd_clear_page_dir_entry(struct Env *e, uint32 virtual_address);
//================

void sched_delete_ready_queues();
uint32 isSchedMethodRR() { if (scheduler_method == SCH_RR) return 1; return 0; }
uint32 isSchedMethodMLFQ() { if (scheduler_method == SCH_MLFQ) return 1; return 0; }

//==================================================================================//
//============================== HELPER FUNCTIONS ==================================//
//==================================================================================//
void init_queue(struct Env_Queue* queue)
{
	if (queue != NULL)
	{
		LIST_INIT(queue);
	}
}

struct Env* find_env_in_queue(struct Env_Queue* queue, uint32 envID)
{
	struct Env * ptr_env = NULL;
	LIST_FOREACH(ptr_env, queue)
	{
		if (ptr_env->env_id == envID)
		{
			return ptr_env;
		}
	}
	return NULL;
}
//==================================================================================//

void sched_init_MLFQ(uint8 numOfLevels, uint8 *quantumOfEachLevel)
{
	//=========================================
	//DON'T CHANGE THESE LINES=================
	//sched_delete_ready_queues();
	scheduler_status = SCH_STOPPED;
	scheduler_method = SCH_MLFQ;
	//=========================================
	//=========================================

	//TODO: [FINAL_EVAL_2020 - VER_B] - [1] MLFQ [Initialize]
	// Write your code here, remove the panic and write your code
	//refer to the documentation for details

	num_of_ready_queues = numOfLevels;

	//[1] Initialize the ready queues using init_queue()
	for (int i = 0; i <= num_of_ready_queues; i++)
	{
		init_queue(&env_ready_queues[i]);
	}
	//[2] Initialize the "quantums" array by the given quantums in "quantumOfEachLevel[]"
	for (int i = 0; i < numOfLevels; i++)
	{
		quantums[i] = quantumOfEachLevel[i];
	}
	//[3] Set the CPU quantum by the first level one
	kclock_set_quantum(quantums[0]);

}

struct Env* fos_scheduler_MLFQ()
{
	//TODO: [FINAL_EVAL_2020 - VER_B] - [1] MLFQ [Scheduler]
	// Write your code here, remove the panic and write your code
	//refer to the documentation for details

	//Apply the MLFQ with the specified levels to pick up the next environment
	//Note: the "curenv" (if exist) should be placed in its correct queue

	//[1] If the current environment (curenv) exists, place it in the suitable queue
	struct Env* element;
	if (curenv != NULL)
	{
		LIST_INSERT_TAIL(&(env_ready_queues[0]), curenv);
	}
	//[2] Search for the next env in the queues according to their priorities (first is highest)
	struct Env* nextEnv;
	int currentLevel;
	for (int j = 0; j <= LIST_SIZE(&(env_ready_queues)); j++)
	{
		if (&env_ready_queues[j].size != 0)
		{
			nextEnv = LIST_FIRST(&env_ready_queues[j]);
			currentLevel = j;
			break;
		}
	}
	//[3] If next env is found: Set the CPU quantum by the quantum of the selected level
	//,remove the selected env from its queue and return it
	//	  Else, return NULL
	if (nextEnv != NULL)
	{
		kclock_set_quantum(quantums[currentLevel]);
		LIST_REMOVE(&env_ready_queues[currentLevel], nextEnv);
		return nextEnv;
	}
	return NULL;
}


void
fos_scheduler(void)
{
	chk1();
	scheduler_status = SCH_STARTED;

	//This variable should be set to the next environment to be run (if any)
	struct Env* next_env = NULL;

	if (scheduler_method == SCH_RR)
	{
		// Implement simple round-robin scheduling.
		// Pick next environment from the ready queue,
		// and switch to such environment if found.
		// It's OK to choose the previously running env if no other env
		// is runnable.

		//If the curenv is still exist, then insert it again in the ready queue
		if (curenv != NULL)
		{
			LIST_INSERT_HEAD(&(env_ready_queues[0]), curenv);
		}

		//Pick the next environment from the ready queue
		next_env = LIST_LAST(&(env_ready_queues[0]));
		if (next_env != NULL)
			LIST_REMOVE(&(env_ready_queues[0]), next_env);

		//Reset the quantum
		//2017: Reset the value of CNT0 for the next clock interval
		kclock_set_quantum(quantums[0]);
		//uint16 cnt0 = kclock_read_cnt0_latch() ;
		//cprintf("CLOCK INTERRUPT AFTER RESET: Counter0 Value = %d\n", cnt0 );

	}
	else if (scheduler_method == SCH_MLFQ)
	{
		next_env = fos_scheduler_MLFQ();
	}


	//temporarily set the curenv by the next env JUST for checking the scheduler
	//Then: reset it again
	struct Env* old_curenv = curenv;
	curenv = next_env;
	chk2(next_env);
	curenv = old_curenv;

	//sched_print_all();

	if (next_env != NULL)
	{
		//		cprintf("\nScheduler select program '%s' [%d]... counter = %d\n", next_env->prog_name, next_env->env_id, kclock_read_cnt0());
		//		cprintf("Q0 = %d, Q1 = %d, Q2 = %d, Q3 = %d\n", queue_size(&(env_ready_queues[0])), queue_size(&(env_ready_queues[1])), queue_size(&(env_ready_queues[2])), queue_size(&(env_ready_queues[3])));
		env_run(next_env);
	}
	else
	{
		/*2015*///No more envs... curenv doesn't exist any more! return back to command prompt
		curenv = NULL;
		//lcr3(K_PHYSICAL_ADDRESS(ptr_page_directory));
		lcr3(phys_page_directory);

		//cprintf("SP = %x\n", read_esp());

		scheduler_status = SCH_STOPPED;
		//cprintf("[sched] no envs - nothing more to do!\n");
		while (1)
			run_command_prompt(NULL);

	}
}

void sched_init_RR(uint8 quantum)
{
	scheduler_status = SCH_STOPPED;
	scheduler_method = SCH_RR;

	// Create 1 ready queue for the RR
	num_of_ready_queues = 1;
#if USE_KHEAP
	sched_delete_ready_queues();
	env_ready_queues = kmalloc(sizeof(struct Env_Queue));
	quantums = kmalloc(num_of_ready_queues * sizeof(uint8));
#endif
	quantums[0] = quantum;
	kclock_set_quantum(quantums[0]);
	init_queue(&(env_ready_queues[0]));
}



void sched_init()
{
	old_pf_counter = 0;

	sched_init_RR(CLOCK_INTERVAL_IN_MS);

	init_queue(&env_new_queue);
	init_queue(&env_exit_queue);
}

void sched_delete_ready_queues()
{
	if (env_ready_queues != NULL)
		kfree(env_ready_queues);
	if (quantums != NULL)
		kfree(quantums);
}



void sched_insert_new(struct Env* env)
{
	if (env != NULL)
	{
		env->env_status = ENV_NEW;
		LIST_INSERT_HEAD(&env_new_queue, env);
	}
}
void sched_remove_new(struct Env* env)
{
	if (env != NULL)
	{
		LIST_REMOVE(&env_new_queue, env);
		env->env_status = ENV_UNKNOWN;
	}
}

void sched_insert_exit(struct Env* env)
{
	if (env != NULL)
	{
		if (isBufferingEnabled()) { cleanup_buffers(env); }
		env->env_status = ENV_EXIT;
		LIST_INSERT_HEAD(&env_exit_queue, env);
	}
}
void sched_remove_exit(struct Env* env)
{
	if (env != NULL)
	{
		LIST_REMOVE(&env_exit_queue, env);
		env->env_status = ENV_UNKNOWN;
	}
}

void sched_print_all()
{
	struct Env* ptr_env;
	if (!LIST_EMPTY(&env_new_queue))
	{
		cprintf("\nThe processes in NEW queue are:\n");
		LIST_FOREACH(ptr_env, &env_new_queue)
		{
			cprintf("	[%d] %s\n", ptr_env->env_id, ptr_env->prog_name);
		}
	}
	else
	{
		cprintf("\nNo processes in NEW queue\n");
	}
	cprintf("================================================\n");
	for (int i = 0; i < num_of_ready_queues; i++)
	{
		if (!LIST_EMPTY(&(env_ready_queues[i])))
		{
			cprintf("The processes in READY queue #%d are:\n", i);
			LIST_FOREACH(ptr_env, &(env_ready_queues[i]))
			{
				cprintf("	[%d] %s\n", ptr_env->env_id, ptr_env->prog_name);
			}
		}
		else
		{
			cprintf("No processes in READY queue #%d\n", i);
		}
		cprintf("================================================\n");
	}
	if (!LIST_EMPTY(&env_exit_queue))
	{
		cprintf("The processes in EXIT queue are:\n");
		LIST_FOREACH(ptr_env, &env_exit_queue)
		{
			cprintf("	[%d] %s\n", ptr_env->env_id, ptr_env->prog_name);
		}
	}
	else
	{
		cprintf("No processes in EXIT queue\n");
	}
}

void sched_run_all()
{
	struct Env* ptr_env = NULL;
	LIST_FOREACH(ptr_env, &env_new_queue)
	{
		sched_remove_new(ptr_env);
		ptr_env->env_status = ENV_READY;
		LIST_INSERT_HEAD(&(env_ready_queues[0]), ptr_env);
	}
	/*2015*///if scheduler not run yet, then invoke it!
	if (scheduler_status == SCH_STOPPED)
		fos_scheduler();
}

void sched_kill_all()
{
	struct Env* ptr_env;
	if (!LIST_EMPTY(&env_new_queue))
	{
		cprintf("\nKILLING the processes in the NEW queue...\n");
		LIST_FOREACH(ptr_env, &env_new_queue)
		{
			cprintf("	killing[%d] %s...", ptr_env->env_id, ptr_env->prog_name);
			sched_remove_new(ptr_env);
			start_env_free(ptr_env);
			cprintf("DONE\n");
		}
	}
	else
	{
		cprintf("No processes in NEW queue\n");
	}
	cprintf("================================================\n");
	for (int i = 0; i < num_of_ready_queues; i++)
	{
		if (!LIST_EMPTY(&(env_ready_queues[i])))
		{
			cprintf("KILLING the processes in the READY queue #%d...\n", i);
			LIST_FOREACH(ptr_env, &(env_ready_queues[i]))
			{
				cprintf("	killing[%d] %s...", ptr_env->env_id, ptr_env->prog_name);
				LIST_REMOVE(&(env_ready_queues[i]), ptr_env);
				start_env_free(ptr_env);
				cprintf("DONE\n");
			}
		}
		else
		{
			cprintf("No processes in READY queue #%d\n", i);
		}
		cprintf("================================================\n");
	}

	if (!LIST_EMPTY(&env_exit_queue))
	{
		cprintf("KILLING the processes in the EXIT queue...\n");
		LIST_FOREACH(ptr_env, &env_exit_queue)
		{
			cprintf("	killing[%d] %s...", ptr_env->env_id, ptr_env->prog_name);
			sched_remove_exit(ptr_env);
			start_env_free(ptr_env);
			cprintf("DONE\n");
		}
	}
	else
	{
		cprintf("No processes in EXIT queue\n");
	}

	//reinvoke the scheduler since there're no env to return back to it
	curenv = NULL;
	fos_scheduler();
}


void sched_new_env(struct Env* e)
{
	//add the given env to the scheduler NEW queue
	if (e != NULL)
	{
		sched_insert_new(e);
	}
}
/*
void sched_new_env(uint32 envId)
{
	struct Env* e= NULL;
	envid2env(envId, &e, 0);

	if(e == 0) return;

	sched_insert_new(e);
}
 */

void sched_run_env(uint32 envId)
{
	struct Env* ptr_env = NULL;
	LIST_FOREACH(ptr_env, &env_new_queue)
	{
		if (ptr_env->env_id == envId)
		{
			sched_remove_new(ptr_env);
			ptr_env->env_status = ENV_READY;
			LIST_INSERT_HEAD(&(env_ready_queues[0]), ptr_env);
			/*2015*///if scheduler not run yet, then invoke it!
			if (scheduler_status == SCH_STOPPED)
			{
				fos_scheduler();
			}
			break;
		}
	}
	//	cprintf("ready queue:\n");
	//	LIST_FOREACH(ptr_env, &env_ready_queue)
	//	{
	//		cprintf("%s - %d\n", ptr_env->prog_name, ptr_env->env_id);
	//	}

}

void sched_exit_env(uint32 envId)
{
	struct Env* ptr_env = NULL;
	int found = 0;
	if (!found)
	{
		LIST_FOREACH(ptr_env, &env_new_queue)
		{
			if (ptr_env->env_id == envId)
			{
				sched_remove_new(ptr_env);
				found = 1;
				//			return;
			}
		}
	}
	if (!found)
	{
		for (int i = 0; i < num_of_ready_queues; i++)
		{
			if (!LIST_EMPTY(&(env_ready_queues[i])))
			{
				ptr_env = NULL;
				LIST_FOREACH(ptr_env, &(env_ready_queues[i]))
				{
					if (ptr_env->env_id == envId)
					{
						LIST_REMOVE(&(env_ready_queues[i]), ptr_env);
						found = 1;
						break;
					}
				}
			}
			if (found)
				break;
		}
	}
	if (!found)
	{
		if (curenv->env_id == envId)
		{
			ptr_env = curenv;
			found = 1;
		}
	}

	if (found)
	{
		sched_insert_exit(ptr_env);

		//If it's the curenv, then reinvoke the scheduler as there's no meaning to return back to an exited env
		if (curenv->env_id == envId)
		{
			curenv = NULL;
			fos_scheduler();
		}
	}
}

/*2018*/
void sched_exit_all_ready_envs()
{
	struct Env* ptr_env = NULL;
	for (int i = 0; i < num_of_ready_queues; i++)
	{
		if (!LIST_EMPTY(&(env_ready_queues[i])))
		{
			ptr_env = NULL;
			LIST_FOREACH(ptr_env, &(env_ready_queues[i]))
			{
				LIST_REMOVE(&(env_ready_queues[i]), ptr_env);
				sched_insert_exit(ptr_env);
			}
		}
	}
}

/*2015*/
void sched_kill_env(uint32 envId)
{
	struct Env* ptr_env = NULL;
	int found = 0;
	if (!found)
	{
		LIST_FOREACH(ptr_env, &env_new_queue)
		{
			if (ptr_env->env_id == envId)
			{
				cprintf("killing[%d] %s from the NEW queue...", ptr_env->env_id, ptr_env->prog_name);
				sched_remove_new(ptr_env);
				start_env_free(ptr_env);
				cprintf("DONE\n");
				found = 1;
				//			return;
			}
		}
	}
	if (!found)
	{
		for (int i = 0; i < num_of_ready_queues; i++)
		{
			if (!LIST_EMPTY(&(env_ready_queues[i])))
			{
				ptr_env = NULL;
				LIST_FOREACH(ptr_env, &(env_ready_queues[i]))
				{
					if (ptr_env->env_id == envId)
					{
						cprintf("killing[%d] %s from the READY queue #%d...", ptr_env->env_id, ptr_env->prog_name, i);
						LIST_REMOVE(&(env_ready_queues[i]), ptr_env);
						start_env_free(ptr_env);
						cprintf("DONE\n");
						found = 1;
						break;
						//return;
					}
				}
			}
			if (found)
				break;
		}
	}
	if (!found)
	{
		ptr_env = NULL;
		LIST_FOREACH(ptr_env, &env_exit_queue)
		{
			if (ptr_env->env_id == envId)
			{
				cprintf("killing[%d] %s from the EXIT queue...", ptr_env->env_id, ptr_env->prog_name);
				sched_remove_exit(ptr_env);
				start_env_free(ptr_env);
				cprintf("DONE\n");
				found = 1;
				//return;
			}
		}
	}

	if (!found)
	{
		if (curenv->env_id == envId)
		{
			ptr_env = curenv;
			assert(ptr_env->env_id == ENV_RUNNABLE);
			cprintf("killing a RUNNABLE environment [%d] %s...", ptr_env->env_id, ptr_env->prog_name);
			start_env_free(ptr_env);
			cprintf("DONE\n");
			found = 1;
			//If it's the curenv, then reset it and reinvoke the scheduler
			//as there's no meaning to return back to a killed env
			//lcr3(K_PHYSICAL_ADDRESS(ptr_page_directory));
			lcr3(phys_page_directory);
			curenv = NULL;
			fos_scheduler();
		}
	}
}


void clock_interrupt_handler()
{
	//cputchar('i');

	if (isPageReplacmentAlgorithmLRUTimeStamp())
	{
		update_WS_time_stamps();
	}
	//cprintf("Clock Handler\n") ;
	fos_scheduler();
}



void update_WS_time_stamps()
{
	struct Env *curr_env_ptr = curenv;

	if (curr_env_ptr != NULL)
	{
		{
			int i;
			for (i = 0; i < (curr_env_ptr->page_WS_max_size); i++)
			{
				if (curr_env_ptr->ptr_pageWorkingSet[i].empty != 1)
				{
					//update the time if the page was referenced
					uint32 page_va = curr_env_ptr->ptr_pageWorkingSet[i].virtual_address;
					//uint32 perm = pt_get_page_permissions(curr_env_ptr, page_va) ;
					uint32 *ptr_table = NULL;
					get_page_table(curr_env_ptr->env_page_directory, (void*)page_va, &ptr_table);
					uint32 perm = ptr_table[PTX(page_va)];

					uint32 oldTimeStamp = curr_env_ptr->ptr_pageWorkingSet[i].time_stamp;

					if (perm & PERM_USED)
					{
						curr_env_ptr->ptr_pageWorkingSet[i].time_stamp = (oldTimeStamp >> 2) | 0x80000000;
						//pt_set_page_permissions(curr_env_ptr, page_va, 0 , PERM_USED) ;
						ptr_table[PTX(page_va)] &= ~PERM_USED;
					}
					else
					{
						curr_env_ptr->ptr_pageWorkingSet[i].time_stamp = (oldTimeStamp >> 2);
					}
				}
			}
		}

		{
			int t;
			for (t = 0; t < __TWS_MAX_SIZE; t++)
			{
				if (curr_env_ptr->__ptr_tws[t].empty != 1)
				{
					//update the time if the page was referenced
					uint32 table_va = curr_env_ptr->__ptr_tws[t].virtual_address;
					uint32 oldTimeStamp = curr_env_ptr->__ptr_tws[t].time_stamp;

					if (pd_is_table_used(curr_env_ptr, table_va))
					{
						curr_env_ptr->__ptr_tws[t].time_stamp = (oldTimeStamp >> 2) | 0x80000000;
						pd_set_table_unused(curr_env_ptr, table_va);
					}
					else
					{
						curr_env_ptr->__ptr_tws[t].time_stamp = (oldTimeStamp >> 2);
					}
				}
			}
		}
	}
}

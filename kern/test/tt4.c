#include <types.h>
#include <lib.h>
#include <wchan.h>
#include <thread.h>
#include <synch.h>
#include <test.h>

static
void print_name(void *junk, unsigned long num)
{
	(void)junk;
	kprintf("Child %ld\n", num);
	return;
}

int threadtest4(int nargs, char **args)
{
	(void)nargs;
	(void)args;

	kprintf("Begining tt4...\n");
	int i, value;
	for(i = 0; i < 10; i++)
	{
		value = thread_fork_join("child", NULL, print_name, NULL, i); 
		//if value != 0, we had an error in our forking
		if(value != 0) 
		{
			panic("Fork failed");
		}
	}

	/*for(i = 0; i < 10; i++)
	{
		kprintf("Thread ID: %d", thread_join());
	}*/

	kprintf("Test was successful\n");	
	return 0;
}

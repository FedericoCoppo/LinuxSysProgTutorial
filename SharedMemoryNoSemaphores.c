/* ============================================================================ **
 **                           Embedded Linux                                     **
 ** ============================================================================ **
 **               +++++++++++++++++++++++++++++++++++++++                        **
 **    Module:    +      SharedMemoryNoSemaphore.c      +                        **
 **               +++++++++++++++++++++++++++++++++++++++                        **
 **                                                                              **
 **  Description: This module implements two process shared memory system        **
 **               without semaphores memory access control                       **
 **                                                                              **
 ** ============================================================================ **
 **         Edit                                      Data           Author      **
 **    First release                                30/03/2018       F.Coppo     **
 ** ============================================================================ */

// Include .
#include <stdio.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

// Define .
#define SHARED_MEM_ID       111
#define BUFFER_SIZE          16
#define OFFSET             6500
#define USLEEP_5_MS        5000
#define USLEEP_2_MS        2000

// Local variables .
static int childPid;
static bool finished;

// Variables initialization .
static void init (void)
{
	// Father loop label .
	finished = false;

	childPid = 0;
}

// Callback linked to SIGUSR1 signal .
static void raceConditionSignaller(int sig_num)
{
	printf( "PARENT: wrong memory sequence read by child process (pid %d)!\n", childPid);

	// Father process ending .
	finished = true;
}

// Memory creation .
static int sharedMemCreation (key_t key)
{
	struct shmid_ds shmds;

	int shmid = shmget(key, sizeof(long long *)*BUFFER_SIZE, 0666 | IPC_CREAT);

	if (shmid >= 0)
	{
		// Info request .
		if (shmctl(shmid, IPC_STAT, &shmds) == 0)
		{
			printf("%d bytes size shared memory created\n", (int) shmds.shm_segsz);
		}
		else
		{
			printf("shmctl error = %d\n", errno);
		}
	}
	else
	{
		printf("PARENT: shared memory segment not found.\n");
		exit(-1);
	}

	return shmid;
}

// Memory context attaching .
static long long * sharedMemAttach(int shmid, int role)
{
	struct shmid_ds shmds;

	// Attach .
	long long * mem = (long long *) shmat(shmid, (const void *)0, 0);

	// Info request .
	if (shmctl(shmid, IPC_STAT, &shmds) == 0)
	{
		printf("%s: context attached (currently %d attaches)\n", ((role == 0) ? "PARENT" : " CHILD"), (int)shmds.shm_nattch);
	}
	else
	{
		printf("%s: shmctl error = %d\n",((role == 0) ? "PARENT" : " CHILD"), errno);
	}

	return mem;
}

// Memory context dettaching .
static void sharedMemDetaches(long long * mem, int shmid, int role)
{
	struct shmid_ds shmds;

	if (shmdt(mem) == -1)
	{
		printf("%s: memory detaching error(%d)\n", ((role == 0) ? "PARENT" : " CHILD"), errno);
	}
	else
	{
		// Update info .
		if(shmctl(shmid, IPC_STAT, &shmds) == 0)
		{
			printf("%s: memory (created by pid %d) detached (currently remaining %d attached)\n", ((role == 0) ? "PARENT" : " CHILD"), (int) shmds.shm_cpid, (int) shmds.shm_nattch);
		}
		else
		{
			printf("%s: shmctl error=%d\n", ((role == 0) ? "PARENT" : " CHILD"), errno);
		}
	}
}

// Main routine .
int main()
{
	long long tmpBuff[BUFFER_SIZE];
	int shmid, retFork, i, status;
	long long * mem = NULL;
	int role = -1;

	// Static label init .
	init();

	// Signal callback registration .
	signal(SIGUSR1, raceConditionSignaller);

	// Shared memory creation .
	shmid = sharedMemCreation(SHARED_MEM_ID);

	// Child creation .
	retFork = fork();

	// Child pid update for father, for child will remain equal zero .
	if (retFork > 0)
	{
		childPid = retFork;
	}

	// Father .
	if (retFork > 0)
	{
		printf("PARENT: process created (pid %d)\n", getpid());

		role = 0;

		// Keep the context .
		mem = sharedMemAttach(shmid, role);

		// Father cyclic write .
		while(!finished)
		{
			// Start of critical section .
			for (i=0; i < BUFFER_SIZE; i++)
			{
				mem[i] = (long long) i + OFFSET;
				mem[i] = mem[i]*2;
				printf("PARENT: write = %u\n", (u_int) (mem[i]/2));
				mem[i] = mem[i]/2;
			}
			// End of critical section .

			usleep(USLEEP_5_MS);
		}

		// Wait child ending before remove memory .
		retFork = wait(&status);

		// Detaching memory .
		sharedMemDetaches(mem, shmid, role);

		// Removing memory .
		if (shmctl( shmid, IPC_RMID, 0 ) == 0)
		{
			printf( "PARENT: memory segment removed\n");
		}
		else
		{
			printf( "PARENT: memory segment removing fail!\n" );
		}
	}
	else if (retFork == 0)
	{
		// Child .
		bool finishChild = false;

		role = 1;

		printf(" CHILD: child process created (pid %d)\n", (int) getpid());

		// Keep identifier of the shared memory segment .
		shmid = shmget(SHARED_MEM_ID, 0, 0);

		// Keep the context .
		mem = sharedMemAttach(shmid, role);

		// Reading loop .
		while(!finishChild)
		{
			// Start of critical section .
			for (i=0; i < BUFFER_SIZE; i++)
			{
				tmpBuff[i] = mem[i];
				printf(" CHILD: read  = %u\n", (u_int) mem[i]);
			}
			// End of critical section .

			// Check for any sequence errors .
			for (i=0; i < BUFFER_SIZE; i++)
			{
				if (tmpBuff[i] != (long long) i + OFFSET)
				{
					// End the child process and signal the father .
					printf(" CHILD: sequence error (expected value : %u, read value : %u), child will exit\n", (u_int) i + OFFSET,  (u_int) tmpBuff[i]);
					finishChild = true;
					kill( getppid(), SIGUSR1);
					break;
				}
			}

			usleep(USLEEP_2_MS);
		}

		// Memory detach .
		sharedMemDetaches(mem, shmid, role);
	}
	else
	{
		printf("CHILD: error trying to fork() (%d)\n", errno);
	}

	printf("%s: Exiting...\n", ((role == 0) ? "PARENT" : " CHILD"));
	fflush(stdout);

	return 0;
}

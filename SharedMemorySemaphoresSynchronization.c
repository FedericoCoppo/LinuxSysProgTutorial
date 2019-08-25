/* ============================================================================ **
 **                           Embedded Linux                                     **
 ** ============================================================================ **
 **             +++++++++++++++++++++++++++++++++++++++++++                      **
 **    Module:  + SharedMemorySemaphoresSynchronization.c +                      **
 **             +++++++++++++++++++++++++++++++++++++++++++                      **
 **                                                                              **
 **  Description: This module implements two process shared memory system        **
 **               with two semaphores Wait-Signal synchronization                **
 **                                                                              **
 ** ============================================================================ **
 **         Edit                                      Data           Author      **
 **    First release                                02/04/2018     F.Coppo       **
 ** ============================================================================ */

// Include .
#include <stdio.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sem.h>
#include <stdlib.h>

// Define .
#define BUFFER_SIZE          16
#define OFFSET            65000
#define USLEEP_20_MS      20000
#define USLEEP_100_MS    100000
#define SHARED_MEM_ID       111
#define SEM_ID_1            112
#define SEM_ID_2            113
#define CYCLE_NUMBER         50

// Local variables .
static int childPid = 0;

// Callback linked to SIGINT signal .
void endProcessesSignaller (int sig_num)
{
	if (childPid == 0)
	{
		// Child kill request .
		printf("Child kill request (pid %d)\n", (int) getpid());
		printf("Child killing...\n");
		kill(getpid(), SIGUSR1);
	}
	else
	{
		// Father kill request: also the child is killed .
		printf("Father kill request (pid %d)\n", (int) getpid());
		printf("Child killing...\n");
		kill(childPid, SIGUSR1);
		printf("Father killing...\n");
		kill(getpid(), SIGUSR1);
	}
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
static bool sharedMemAttach (int shmid, int role, long long * * ptPtMem)
{
	struct shmid_ds shmds;
	bool success = true;

	// Attach shmid memory .
	*ptPtMem = (long long *) shmat(shmid, (const void *)0, 0);

	// Info request .
	if (shmctl(shmid, IPC_STAT, &shmds) == 0)
	{
		printf("%s: context attached (currently %d attaches)\n", ((role == 0) ? "PARENT" : " CHILD"), (int) shmds.shm_nattch);
	}
	else
	{
		printf("%s: shmctl error = %d\n",((role == 0) ? "PARENT" : " CHILD"), errno);
		success = false;
	}

	return success;
}

// Memory context detaching .
static void sharedMemDetaches(long long * ptMem, int shmid, int role)
{
	struct shmid_ds shmds;

	if (shmdt(ptMem) == -1)
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

// Binary semaphore creation .
static int semCreate (key_t key)
{
	int semid;

	semid = semget(key, 1, 0666 | IPC_CREAT );

	if (semid != -1)
	{
		printf( "Semaphore %d has been created\n", semid);
	}

	return semid;
}

// Binary semaphore removing .
static void semDelete (int semid)
{
	int res = semctl(semid, 0, IPC_RMID);

	if (res != -1)
	{
		printf("Semaphore %d removed.\n", semid);
	}
}

// Binary semaphore value setting .
int semSetVal (int semid, int value)
{
	return semctl(semid, 0, SETVAL, value);
}

// Binary semaphore acquire .
void semWait (int semid, int role)
{
	struct sembuf sb;

	sb.sem_num = 0;
	sb.sem_op = -1;
	sb.sem_flg = 0;

	if ( semop(semid, &sb, 1) == -1 )
	{
		printf("%s: semaphore %d acquisition failed.\n", ((role == 0) ? "PARENT" : " CHILD"), semid);
		exit(-1);
	}
}

// Binary semaphore release .
void semSignal(int semid, int role)
{
	struct sembuf sb;

	sb.sem_num = 0;
	sb.sem_op = 1;
	sb.sem_flg = 0;

	if ( semop(semid, &sb, 1) == -1 )
	{
		printf("%s: semaphore %d acquisition failed.\n", ((role == 0) ? "PARENT" : " CHILD"), semid);
		exit(-1);
	}
}

// Main routine .
int main()
{
	long long tmpBuff[BUFFER_SIZE];
	int shmid, retFork, i, status;
	long long * mem = NULL;
	int role = -1;
	int semid1;
	int semid2;
	unsigned int cycle = CYCLE_NUMBER;

	// Signal callback registration .
	signal(SIGINT, endProcessesSignaller);

	// Shared memory create .
	shmid = sharedMemCreation(SHARED_MEM_ID);

	// Semaphore1 create .
	semid1 = semCreate(SEM_ID_1);

	// Semaphore 1 unlock (set value equal 1).
	if (semid1 >= 0)
	{
		if (semSetVal(semid1, 1) != -1)
		{
			printf("Semaphore %d count = %d.\n", semid1, semctl(semid1, 0, GETVAL));
		}
	}
	else
	{
		printf("Semaphore creation error\n");
		exit(-1);
	}

	// Semaphore 2 create (leave value equal 0).
	semid2 = semCreate(SEM_ID_2);

	// Semaphore unlock .
	if (semid2 >= 0)
	{
		printf("Semaphore %d count = %d.\n", semid2, semctl( semid2, 0, GETVAL ));
	}
	else
	{
		printf("Semaphore creation error\n");
		exit(-1);
	}

	// Child creation .
	retFork = fork();

	// Child pid update .
	if (retFork > 0)
	{
		childPid = retFork;
	}

	// Father .
	if (retFork > 0)
	{
		printf("PARENT: process created (pid %d)\n", (int) getpid());

		role = 0;

		// Keep the context .
		if ( sharedMemAttach(shmid, role, &mem) )
		{
			// Father cyclic write .
			while(cycle--)
			{
				// Acquire semaphore .
				semWait(semid1, role);

				// Start of critical section .
				for (i=0; i < BUFFER_SIZE; i++)
				{
					mem[i] = (long long) i + OFFSET;
					mem[i] = mem[i]*2;
					printf("PARENT: write = %u\n", (u_int) (mem[i]/2));
					mem[i] = mem[i]/2;
				}
				// End of critical section .

				// Release semaphore .
				semSignal(semid2, role);

				usleep(USLEEP_20_MS);
			}
		}

		// Wait child ending before delete memory .
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

		// Semaphores delete .
		semDelete(semid1);
		semDelete(semid2);
	}
	else if (retFork == 0)
	{
		// Child .
		role = 1;

		printf(" CHILD: child process created (pid %d)\n", getpid());

		// Keep identifier of the shared memory segment .
		shmid = shmget(SHARED_MEM_ID, 0, 0);

		// Keep the context .
		if ( sharedMemAttach(shmid, role, &mem) )
		{
			// Reading loop .
			while(cycle--)
			{
				// Acquire semaphore .
				semWait(semid2, role);

				// Start of critical section .
				for (i=0; i < BUFFER_SIZE; i++)
				{
					tmpBuff[i] = mem[i];
					printf(" CHILD: read  = %u\n", (u_int) tmpBuff[i]);
				}
				// End of critical section .

				// Release semaphore .
				semSignal(semid1, role);

				// Values pattern control (out from critical section) .
				for (i=0; i < BUFFER_SIZE; i++)
				{
					// Check wrong read (child know the sequence) .
					if (tmpBuff[i] != (long long) i + OFFSET)
					{
						printf(" CHILD: sequence error (expected value : %u, read value : %u), child will exit\n", (u_int) i + OFFSET,  (u_int) tmpBuff[i]);
						break;
					}
				}

				usleep(USLEEP_100_MS);
			}
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


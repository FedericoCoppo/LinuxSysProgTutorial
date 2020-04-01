
# Description
This is a simple example of process, callback, shared memory and semaphore (Unix system V) usage on Linux system programming.

# Environment
Ubuntu 16.04 
Eclipse cross GCC

# Code
```
SharedMemoryNoSemaphores.c 
```
the programm create 2 process (forks routine) that access to a shared memory (read/write) without any mutual exclusion; 
the program is able to detect the race condition.

```
SharedMemorySemaphore.c 
```
the program adds one semaphore around the critical section that allow mutual exclusion

```
SharedMemorySemaphoresSyncronization.c
```
the program implements two semaphores in wait-signal configuration to allow producer/consumer sync

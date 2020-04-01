
# Description
This is a simple example of process, callback, shared memory and semaphore (Unix system V) usage on Linux system programming.

# Environment
Ubuntu 16.04 and Eclipse cross GCC.

# Code
```
SharedMemoryNoSemaphores.c 
```
The programm create 2 process (forks routine) that access to a shared memory (read/write) without any mutual exclusion. 
The program is able to detect the race condition.

```
SharedMemorySemaphore.c 
```
The program adds one semaphore around the critical section that allow mutual exclusion.

```
SharedMemorySemaphoresSyncronization.c
```
The program implements two semaphores in wait-signal configuration to allow producer/consumer sync.

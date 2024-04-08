#ifndef schedulingLogic_h
#define schedulingLogic_h

#include <stdbool.h>

#include "graph.h"
#include "stats.h"
#include "simulation.h"

// Cannot include computer.h because of circular dependency
// -> forward declaration of Computer
typedef struct Computer_t Computer;


/* ---------------------------- Scheduler struct ---------------------------- */

typedef struct Scheduler_t Scheduler;


/* -------------------------- getters and setters -------------------------- */

int getWaitQueueCount(void);



/* -------------------------- init/free functions -------------------------- */

Scheduler *initScheduler(SchedulingAlgorithm **readyQueueAlgorithms, int readyQueueCount);
void freeScheduler(Scheduler *scheduler);


/* -------------------------- scheduling functions ------------------------- */

/**
 * Add a new process to the first queue of the scheduler given its PID if it is not already in the scheduler.
 *
 * @param scheduler: the scheduler
 * @param pid: the pid of the new process
 */
void addProcessToReadyQueue(Scheduler *scheduler, PCB *data);

/**
 * Add a process to the waiting queue of the scheduler given its pcb
 * 
 * @param scheduler: the scheduler
 * @param pid: the pid of the process
 */
void addProcessToWaitingQueue(Scheduler *scheduler, int pid);

/**
 * Check all the process in the ready queues to see if a limit is reached, 
 * if so move the process from queue accordingly to the limit that has been reached
 * 
 * @param scheduler: the scheduler
 */
void schedulingEvents(Scheduler *scheduler);

/**
 * return the pid of the process with the highest priority in the scheduler
 * 
 * @param scheduler: the scheduler
 * @return the pid of the process with the highest priority
 */
int scheduling(Scheduler *scheduler);

void setProcessToCore(Computer *computer, int indexCore, int pid);

void updateSchedulingValue(Scheduler *scheduler);

void removeProcessFromScheduler(Computer *computer, int pid, int indexCore);

void printQueue(Scheduler *scheduler);

#endif // schedulingLogic_h

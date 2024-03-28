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

/**
 * Get the PID from the first queue which is not empty (to respect the priority queue)
 * 
 * @param scheduler the scheduler
 * @return int: the pid
 */
int getPIDfromReadyQueue(Scheduler *scheduler);

/* -------------------------- init/free functions -------------------------- */

Scheduler *initScheduler(SchedulingAlgorithm **readyQueueAlgorithms, int readyQueueCount);
void freeScheduler(Scheduler *scheduler);


/* -------------------------- scheduling functions ------------------------- */

/**
 * Add a new process to the scheduler
 *
 * @param scheduler: the scheduler
 * @param pid: the pid of the new process
 */
void addProcessToScheduler(Scheduler *scheduler, int pid);

void scheduling(Scheduler *scheduler);

void printQueue(Scheduler *scheduler);

#endif // schedulingLogic_h

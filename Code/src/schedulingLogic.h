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
void addProcessToScheduler(Scheduler *scheduler, PCB *data);

void schedulingEvents(Scheduler *scheduler);

void testScheduling(Scheduler *scheduler);

void printQueue(Scheduler *scheduler);

#endif // schedulingLogic_h

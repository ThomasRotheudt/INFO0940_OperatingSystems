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

/* ------------------- Change Process of Queue -------------------- */

/**
 * Add a new process to the first queue of the scheduler given its PID if it is not already in the scheduler.
 *
 * @param scheduler: the scheduler
 * @param pid: the pid of the new process
 */
void addProcessToScheduler(Scheduler *scheduler, PCB *data);

/**
 * Add a process to the waiting queue of the scheduler given its pcb
 * 
 * @param scheduler: the scheduler
 * @param pid: the pid of the process
 */
void addProcessToWaitingQueue(Scheduler *scheduler, int pid);

void removeProcessFromScheduler(Computer *computer, int pid, int indexCore);

void returnFromWaitQueue(Scheduler *scheduler, int pid);

/* -------------------- Event & Update values --------------------- */

/**
 * Check all the process in the ready queues to see if a limit is reached, 
 * if so move the process from queue accordingly to the limit that has been reached
 * 
 * @param scheduler: the scheduler
 */
void schedulingEvents(Scheduler *scheduler, Computer *computer, Workload *workload);

void updateSchedulingValue(Scheduler *scheduler, Workload *workload);

void preemption(Computer *computer, Workload *workload);

/* --------------------- Assign Ressources ------------------------ */

/**
 * return the pid of the process with the highest priority in the scheduler
 * 
 * @param scheduler: the scheduler
 * @return the pid of the process with the highest priority
 */
int scheduling(Scheduler *scheduler);

void setProcessToCore(Computer *computer, int indexCore, int pid);

void setProcessToDisk(Computer *computer);



void printQueue(Scheduler *scheduler);

#endif // schedulingLogic_h

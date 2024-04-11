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
 * Add a process to the waiting queue of the scheduler given its pid
 * 
 * @param scheduler: The scheduler
 * @param pid: The pid of the process
 */
void addProcessToWaitingQueue(Scheduler *scheduler, int pid);

/**
 * Remove a process from the waiting queue and puts it in its ready queue given its pid
 * 
 * @param scheduler: The scheduler 
 * @param pid: The pid of the process
 */
void returnFromWaitQueue(Scheduler *scheduler, int pid);

/**
 * Remove a process from the scheduler and set the core where it was running to idle 
 * 
 * @param computer: All composants of the computer (scheduler, cpu, and disk)
 * @param pid: The pid of the process to remove
 * @param indexCore: The index of the core to change
 */
void removeProcessFromScheduler(Computer *computer, int pid, int indexCore);

/* -------------------- Event & Update values --------------------- */

/**
 * Check all the process in the ready queues and the running queue to see if a limit or a timer (Round-Robin) is reached, 
 * if so handle the event (timer is finished, limit is reached)
 * 
 * @param scheduler: All composants of the computer (scheduler, cpu, and disk)
 * @param workload: The workload
 */
void schedulingEvents(Computer *computer, Workload *workload, AllStats *stats);


/**
 * Update all value in the ready queue and the running queue (RR timer, age, and execution time in the queue)
 * 
 * @param scheduler: The scheduler
 * @param workload: The workload
 */
void updateSchedulingValue(Scheduler *scheduler, Workload *workload, AllStats *stats);

/**
 * Check if a process must be preempted (if a process with higher priority is in the scheduler).
 * Preempt the core if there is one.
 * 
 * @param computer: All composants of the computer (scheduler, cpu, and disk)
 * @param workload: The workload
 */
void preemption(Computer *computer, Workload *workload, AllStats *stats);

/* --------------------- Assign Ressources ------------------------ */

/**
 * Set a process to a core given by the scheduler (thanks to the scheduling algorithm)
 * 
 * @param computer: All composants of the computer (scheduler, cpu, and disk)
 * @param indexCore: The index of the core to change
 * @param pid: The pid of the proces
 */
void setProcessToCore(Computer *computer, Workload *workload, int indexCore, AllStats *stats);

/**
 * Set the first process of the waiting queue to the disk
 * 
 * @param computer: All composants of the computer (scheduler, cpu, and disk)
 */
void setProcessToDisk(Computer *computer);



void printQueue(Scheduler *scheduler);

#endif // schedulingLogic_h

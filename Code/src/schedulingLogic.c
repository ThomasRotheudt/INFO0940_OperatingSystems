// This is where you should implement most of your code.
// You will have to add function declarations in the header file as well to be
// able to call them from simulation.c.

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "computer.h"
#include "schedulingLogic.h"
#include "utils.h"
#include "schedulingAlgorithms.h"

#define NB_WAIT_QUEUES 1
#define FIRST_QUEUE 0
#define WAITING_QUEUE -1
#define RUNNING_QUEUE -2
/* --------------------------- struct definitions -------------------------- */

/**
 * The Queue structure represents a scheduler queue. This queue can be a ready queue or a waiting queue. 
 * These queues contain the PCB of the processes.
 */
typedef struct Queue_t Queue;

/**
 * The QueueNode struct represents a element of the scheduler's queues, here it is a process's PCB. 
 * It contains a pointer to the next element of the queue, the limits, the timer of a round robin slice,
 * the index of its ready queue, and a data field which is the PCB
 */
typedef struct QueueNode_t QueueNode;

struct QueueNode_t
{
    PCB *data; // PCB of the process in the node
    int indexReadyQueue; // The index of the queue in which the node is (use it when returns from waiting queue) 
    int age; // The time the process has spent in its current queue
    int execTime; // The execution time limit on the queue
    int RRTimer; // The timer of the current RR slice advancement
    QueueNode *nextNode;
    QueueNode *prevNode;
};

struct Queue_t
{
    int index;
    int nbrOfNode;
    QueueNode *tail;
    QueueNode *head;
};

struct Scheduler_t
{
    // This is not the ready queues, but the ready queue algorithms
    SchedulingAlgorithm **readyQueueAlgorithms;
    Queue **readyQueue; // Array of queue for the ready queues
    Queue *runningQueue; // Queue whose contents are running process
    Queue *waitingQueue; // Queue whose contents are process waiting for I/O interrupts
    int readyQueueCount;
};

/* ---------------------------- static functions --------------------------- */


/* ---------------- AddProcessToScheduler static function ----------- */

/**
 * Check if the process is already in the scheduler (ready, running, waiting queues) given its pid
 * 
 * @param scheduler: The scheduler
 * @param pid: The pid of the process
 * @return True if the scheduler is the scheduler, False otherwise
 */
static bool isInScheduler(Scheduler *scheduler, int pid);

/* ---------------- schedulingEvents static function ---------------- */
/*These functions only serve in the schedulingEvents function for readability*/

/**
 * Check the events in the ready queues. (limit age is reached or not)
 * 
 * @param scheduler: The scheduler
 */
static void checkEventsInReadyQueue(Scheduler *scheduler);

/**
 * Check for events in the running queue and handle them accordingly.
 * 
 * @param computer: All composants of the computer (scheduler, cpu, and disk)
 * @param workload: The workload
 */
static void checkEventsInRunningQueue(Computer *computer, Workload *workload, AllStats *stats);

/**
 * Find the core on the cpu that runs the process given its pid
 * 
 * @param cpu: The cpu
 * @param pid: The pid of the process
 * @return A pointer to the core that runs the process, NULL otherwise
 */
static Core *findProcessCore(CPU *cpu, int pid);

/* ---------------- setProcessToCore static function ---------------- */

static int scheduling(Scheduler *scheduler, Workload *workload);

/**
 * Apply the First Come First Serve algorithm to the queue.
 * 
 * @param queue: The queue
 * @return The pid of queue's head (First Come), -1 if queue is empty
 */
static int fcfsAlgorithm(Queue *queue);

/**
 * Apply the Round-Robin algorithm to the queue. Just serve the first process of the queue.
 * The algorithm is handle by the scheduling events (check the RRSlice)
 * 
 * @param queue: The queue
 * @return The pid of queue's head, -1 if queue is empty
 */
static int rrAlgorithm(Queue *queue);

/**
 * Apply the Shortest Job First algorithm to the queue. 
 * Serve the process with the shortest remaining execution time of its current CPU burst 
 * 
 * @param queue: The queue
 * @param workload: The workload to get the remaininf time
 * @return The pid of process with the shortest remaining execution time, -1 if queue is empty
 */
static int sjfAlgorithm(Queue *queue, Workload *workload);

/**
 * Apply the Priority algorithm to the queue.
 * Serve the process with the highest priority
 * 
 * @param queue: The queue
 * @return The pid of process with the highest priority, -1 if queue is empty
 */
static int priorityAlgorithm(Queue *queue);


/* ---------------- static Init/free Queue functions  --------------- */

/**
 * Initialize a queue
 * 
 * @param indexQueue: The index of the queue
 * @return A pointer to the new queue
 */
static Queue *initQueue(int indexQueue);

/**
 * Initialize a queue node
 *
 * @param pcb: The pcb of the process in the node
 * @param indexReadyQueue: The index of the ready queue in which the process is
 * @return A pointer to the new node
 */
static QueueNode *initQueueNode(PCB *pcb, int indexReadyQueue);

static void freeQueue(Queue *queue);

/* ---------------- static Queue functions  ------------------------- */

/**
 * Insert a node in a queue
 * 
 * @param queue: The queue in which the node will be inserted
 * @param node: The node to insert
 * @return 1 if the node is inserted, 0 otherwise
 */
static int insertInQueue(Queue *queue, QueueNode *node);

/**
 * Search a node in the queue
 * 
 * @param queue: The queue to search in
 * @param pid: The pid of the process to search
 * @return A pointer to the node, NULL if node is not in the queue 
 */
static QueueNode *searchInQueue(Queue *queue, int pid);

/**
 * Remove a node from a queue
 * 
 * @param queue: The queue from which the node will be removed
 * @param node: The node to remove
 * @return A pointer to the node that is removeds
 */
static QueueNode *removeElement(Queue *queue, QueueNode *node);

/* -------------------------- getters and setters -------------------------- */

int getWaitQueueCount(void)
{
    return NB_WAIT_QUEUES;
}

/* -------------------------- init/free functions -------------------------- */

Scheduler *initScheduler(SchedulingAlgorithm **readyQueueAlgorithms, int readyQueueCount)
{
    Scheduler *scheduler = malloc(sizeof(Scheduler));
    if (!scheduler)
    {
        return NULL;
    }

    scheduler->readyQueueAlgorithms = readyQueueAlgorithms;
    scheduler->readyQueueCount = readyQueueCount;

    // Allocation ready queue
    scheduler->readyQueue = malloc(readyQueueCount * sizeof(Queue*));
    if(!scheduler->readyQueue)
    {
        for (int i = 0; i < scheduler->readyQueueCount; i++)
        {
            free(scheduler->readyQueueAlgorithms[i]);
        }
        free(scheduler->readyQueueAlgorithms);
        free(scheduler);
        return NULL;
    }

    // Initialisation of ready queues
    for (int i = 0; i < readyQueueCount; i++)
    {
        scheduler->readyQueue[i] = initQueue(i);
        if(!scheduler->readyQueue[i])
        {
            for (int j = 0; j <= i; j++)
            {
                freeQueue(scheduler->readyQueue[j]);
            }
            free(scheduler->readyQueue);

            for (int k = 0; k < scheduler->readyQueueCount; k++)
            {
                free(scheduler->readyQueueAlgorithms[k]);
            }
            free(scheduler->readyQueueAlgorithms);

            free(scheduler);
            return NULL;
        }
    }

    // Init of the running queue
    scheduler->runningQueue = initQueue(RUNNING_QUEUE);
    if(!scheduler->runningQueue)
    {
        // Freeing ready queues
        for (int i = 0; i < scheduler->readyQueueCount; i++)
        {
            freeQueue(scheduler->readyQueue[i]);
        }
        free(scheduler->readyQueue);

        // Freeing ready queue algorithms
        for (int i = 0; i < scheduler->readyQueueCount; i++)
        {
            free(scheduler->readyQueueAlgorithms[i]);
        }
        free(scheduler->readyQueueAlgorithms);

        free(scheduler);
        return NULL;
    }

    //Init of the wait queue
    scheduler->waitingQueue = initQueue(WAITING_QUEUE);
    if(!scheduler->waitingQueue)
    {
        // Freeing ready queues
        for (int i = 0; i < scheduler->readyQueueCount; i++)
        {
            freeQueue(scheduler->readyQueue[i]);
        }
        free(scheduler->readyQueue);

        // Freeing ready queue algorithms
        for (int i = 0; i < scheduler->readyQueueCount; i++)
        {
            free(scheduler->readyQueueAlgorithms[i]);
        }
        free(scheduler->readyQueueAlgorithms);

        freeQueue(scheduler->runningQueue);

        free(scheduler);
        return NULL;
    }

    return scheduler;
}



void freeScheduler(Scheduler *scheduler)
{
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        free(scheduler->readyQueueAlgorithms[i]);
    }
    free(scheduler->readyQueueAlgorithms);
    
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        freeQueue(scheduler->readyQueue[i]);
    }
    free(scheduler->readyQueue);

    freeQueue(scheduler->runningQueue);
    freeQueue(scheduler->waitingQueue);
    free(scheduler);
}


/* -------------------------- scheduling functions ------------------------- */

/* ------------------- Change Process of Queue -------------------- */

void addProcessToScheduler(Scheduler *scheduler, PCB *data)
{
    if (!scheduler)
    {
        return;
    }

    // Check if the new process is already in the scheduler
    if (!isInScheduler(scheduler, data->pid))
    {
        // Initialize the new node
        QueueNode *newNode = initQueueNode(data, FIRST_QUEUE);
        if (!newNode) // Check if the new node is created or not
        {
            fprintf(stderr, "Error: The new node can't be created.\n");
            return;
        }

        // Check if the first queue algorithm is RR to set the RR timer
        if (scheduler->readyQueueAlgorithms[FIRST_QUEUE]->type == RR)
        {
            newNode->RRTimer = 0;
        }

        // Insert the new node in the first ready queue of the scheduler
        insertInQueue(scheduler->readyQueue[newNode->indexReadyQueue], newNode);
    }
}


void returnFromRunningQueue(Scheduler *scheduler, int pid)
{
    // Check if the scheduler exist (is allocated)
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exist.\n");
        return;
    }

    // Search the node in the running queue (Only Running -> Waiting), NULL if not in the queue
    QueueNode *node = searchInQueue(scheduler->runningQueue, pid);
    if (node)
    {
        // Remove the node from the Running Queue
        node = removeElement(scheduler->runningQueue, node);
        // Insert the removed node in the waiting queue
        insertInQueue(scheduler->readyQueue[node->indexReadyQueue], node);
    }
}


void addProcessToWaitingQueue(Scheduler *scheduler, int pid)
{
    // Check if the scheduler exist (is allocated)
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exist.\n");
        return;
    }

    // Search the node in the running queue (Only Running -> Waiting), NULL if not in the queue
    QueueNode *node = searchInQueue(scheduler->runningQueue, pid);
    if (node)
    {
        // Remove the node from the Running Queue
        node = removeElement(scheduler->runningQueue, node);
        // Insert the removed node in the waiting queue
        insertInQueue(scheduler->waitingQueue, node);
    }
}


void returnFromWaitQueue(Scheduler *scheduler, int pid)
{
    // Check if the scheduler exist (is allocated)
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exist.\n");
        return;
    }

    // Search the node in the waiting queue (Only waiting -> ready), NULL if not in the queue
    QueueNode *node = searchInQueue(scheduler->waitingQueue, pid);
    if (node)
    {
        if (node->RRTimer > 0)
        {
            node->RRTimer = 0;
        }
        
        // Remove the node from the Waiting Queue
        node = removeElement(scheduler->waitingQueue, node);
        // Insert the removed node in the ready queue of the node
        insertInQueue(scheduler->readyQueue[node->indexReadyQueue], node);
    }
}


void removeProcessFromScheduler(Computer *computer, int pid, int indexCore)
{
    // Check if the computer exist (is allocated)
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }

    // Get the scheduler and cpu in special variables
    Scheduler *scheduler = computer->scheduler;
    CPU *cpu = computer->cpu;

    // Search the node in the Running queue (Only in Running queue), NULL if not in the queue
    QueueNode *node = searchInQueue(scheduler->runningQueue, pid);
    if (node)
    {
        // Remove the node from the Running queue
        node = removeElement(scheduler->runningQueue, node);
        // Free the node
        free(node);
    }

    // Get the core where the process was running
    Core *core = cpu->cores[indexCore];
    // Remove the pid from the core
    core->pid = -1;
    // set the IDLE state to the core
    core->state = IDLE;
}


/* -------------------- Event & Update values --------------------- */

void schedulingEvents(Computer *computer, Workload *workload, AllStats *stats)
{
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exists.\n");
        return;
    }

    // Check for events in ready queue
    checkEventsInReadyQueue(computer->scheduler);
    
    
    // Check for events in running queue
    checkEventsInRunningQueue(computer, workload, stats);
}


void preemption(Computer *computer, Workload *workload, AllStats *stats)
{
    // Check if the computer object exists
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }

    // Retrieve the scheduler and CPU from the computer object
    Scheduler *scheduler = computer->scheduler;
    CPU *cpu = computer->cpu;
    
    // Check all processes running on each core to see if a higher priority process is available
    for (int i = 0; i < cpu->coreCount; i++)
    {
        Core *core = cpu->cores[i];
        // If the core is working (running a process)
        if (core->state == WORKING)
        {
            // Get the statistics for the currently running process
            ProcessStats *processStat = getProcessStats(stats, core->pid);
            // Search for the node of the currently running process in the running queue
            QueueNode *node = searchInQueue(scheduler->runningQueue, core->pid);
            // If the node is found
            if (node)
            {
                // Get the scheduling algorithm info for the node's queue
                SchedulingAlgorithm *algorithmInfo = scheduler->readyQueueAlgorithms[node->indexReadyQueue];
                // Loop through all queues up to the node's queue index
                for (int j = 0; j <= node->indexReadyQueue; j++)
                {
                    // If the current queue is not the node's queue
                    if (j < node->indexReadyQueue)
                    {
                        // Check if there are processes in the queue
                        if (scheduler->readyQueue[j]->nbrOfNode > 0)
                        {
                            // Reset the Round-Robin timer if the current algorithm is RR
                            if (algorithmInfo->type == RR)
                            {   
                                node->RRTimer = 0;
                            }
                            // Set the context switch out of the core
                            core->timer = SWITCH_OUT_DURATION;
                            // Set the state of the core to context switching out
                            core->state = CONTEXT_SWITCHING_OUT;
                            // Set the state of the preempted process to ready
                            setProcessState(workload, node->data->pid, READY);
                            // Increment the number of context switches in statistics
                            processStat->nbContextSwitches++;
                        }
                    }
                    // If the current queue is the node's queue
                    else if (j == node->indexReadyQueue)
                    {
                        int pid = node->data->pid;
                        // If the algorithm is SJF
                        if (algorithmInfo->type == SJF)
                        {
                            // Get the PID of the next shortest job
                            int newPid = sjfAlgorithm(scheduler->readyQueue[node->indexReadyQueue], workload);
                            // If no new process found, exit loop
                            if (newPid == -1)
                            {
                                break;
                            }
                            // If the new job has shorter execution time, preempt
                            if (getProcessCurEventTimeLeft(workload, pid) > getProcessCurEventTimeLeft(workload, newPid))
                            {
                                // Set the context switch out of the core
                                core->timer = SWITCH_OUT_DURATION;
                                // Set the state of the core to context switching out
                                core->state = CONTEXT_SWITCHING_OUT;
                                setProcessState(workload, node->data->pid, READY);
                                processStat->nbContextSwitches++;
                            }
                        }
                        // If the algorithm is Priority
                        else if (algorithmInfo->type == PRIORITY)
                        {
                            // Get the PID of the next highest priority process
                            int newPid = priorityAlgorithm(scheduler->readyQueue[node->indexReadyQueue]);
                            // Search for the node of the new process in its queue
                            QueueNode *newNode = searchInQueue(scheduler->readyQueue[node->indexReadyQueue], newPid);
                            // If the new process exists and has higher priority, preempt
                            if (newNode)
                            {
                                if (node->data->priority > newNode->data->priority)
                                {
                                    // Set the context switch out of the core
                                    core->timer = SWITCH_OUT_DURATION;
                                    // Set the state of the core to context switching out
                                    core->state = CONTEXT_SWITCHING_OUT;
                                    setProcessState(workload, node->data->pid, READY);
                                    processStat->nbContextSwitches++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}



void updateSchedulingValue(Scheduler *scheduler, Workload *workload, AllStats *stats)
{
    // Check if the scheduler exists
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exist.\n");
        return;
    }

    // Update values on ready queues
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        Queue *queue = scheduler->readyQueue[i];

        QueueNode *current = queue->head;
        while (current)
        {
            // Retrieve statistics for the current process
            ProcessStats *processStat = getProcessStats(stats, current->data->pid);

            // Increment waiting time and turnaround time for the process
            processStat->waitingTime++;
            processStat->turnaroundTime++;

            // Increment the age of the process in the ready queue
            current->age++;

            // Move to the next process in the ready queue
            current = current->nextNode;
        }
    }

    // Update values on the running queue
    QueueNode *current = scheduler->runningQueue->head;
    while (current)
    {
        // Retrieve scheduling algorithm information for the current process
        SchedulingAlgorithm *algorithmInfo = scheduler->readyQueueAlgorithms[current->indexReadyQueue];

        // Check if the current process is running
        if (getProcessState(workload, current->data->pid) == RUNNING)
        {
            // If the algorithm has an execution time limit, increment the execution time of the process
            if (algorithmInfo->executiontTimeLimit > 0)
            {
                current->execTime++;
            }

            // If the algorithm is Round Robin, increment the Round Robin timer of the process
            if (algorithmInfo->type == RR)
            {
                current->RRTimer++;
            }
        }

        // Move to the next process in the running queue
        current = current->nextNode;
    }
}


/* --------------------- Assign Ressources ------------------------ */

void setProcessToCore(Computer *computer, Workload *workload, int indexCore, AllStats *stats)
{
    // Check if the computer exists
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }
    
    // Retrieve scheduler and core information from the computer
    Scheduler *scheduler = computer->scheduler;
    Core *core = computer->cpu->cores[indexCore];

    // Get the next process to schedule
    int pid = scheduling(scheduler, workload);
    // If no process is available, return
    if (pid == -1)
    {
        return;
    }
    
    // Iterate through the ready queues of the scheduler
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        // Retrieve statistics for the process
        ProcessStats *processStat = getProcessStats(stats, pid);
        // Search for the process in the ready queue
        QueueNode *node = searchInQueue(scheduler->readyQueue[i], pid);
        // If the process is found in the ready queue
        if (node)
        {
            // Check if the core is idle
            if (core->state == IDLE)
            {
                // Remove the process from the ready queue
                node = removeElement(scheduler->readyQueue[i], node);
                // Insert the process into the running queue
                insertInQueue(scheduler->runningQueue, node);

                // Update core information
                core->pid = pid;
                core->state = CONTEXT_SWITCHING_IN;
                core->timer = SWITCH_IN_DURATION;

                // Update process statistics
                processStat->meanResponseTime += processStat->turnaroundTime; // Add the current time waiting in the queue at the mean response time
                processStat->turnaroundTime = 0; // Use this to compute the number of time waiting in the ready queue before switching in
                processStat->finishTime++; // Use this to compute the number of switch in
            }
        }
    }
}



void setProcessToDisk(Computer *computer)
{
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }

    Scheduler *scheduler = computer->scheduler;
    Disk *disk = computer->disk;

    if (scheduler->waitingQueue->nbrOfNode > 0)
    {
        QueueNode *node = scheduler->waitingQueue->head;
        disk->isIdle = false;
        disk->isFree = false;
        disk->pid = node->data->pid;
    }
}

/* ---------------------------- static functions --------------------------- */

/* ---------------- AddProcessToScheduler static function ----------- */

static bool isInScheduler(Scheduler *scheduler, int pid)
{
    // Check if scheduler is not NULL
    if (!scheduler)
    {
        return false;
    }

    // Iterate over all ready queues
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        if (searchInQueue(scheduler->readyQueue[i], pid))
        {
            return true;
        }
    }

    // Check the running queue
    if (searchInQueue(scheduler->runningQueue, pid))
    {
        return true;
    }

    // Check the wait queue
    if (searchInQueue(scheduler->waitingQueue, pid))
    {
        return true;
    }

    // If we iterated over all queues without finding a match, return false
    return false;
}

/* ---------------- schedulingEvents static function ---------------- */

static Core *findProcessCore(CPU *cpu, int pid)
{
    if (!cpu || pid == -1)
    {
        return NULL;
    }

    // Search the core on which the process run
    for (int i = 0; i < cpu->coreCount; i++)
    {
        if (cpu->cores[i]->state == WORKING)
        {
            if (pid == cpu->cores[i]->pid)
            {
                return cpu->cores[i];
            }
        }
    }
    
    return NULL;
}

static void checkEventsInReadyQueue(Scheduler *scheduler)
{
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        Queue *queue = scheduler->readyQueue[i];
        SchedulingAlgorithm *algorithmInfo = scheduler->readyQueueAlgorithms[i];

        if (algorithmInfo->ageLimit > 0)
        {
            QueueNode *current = queue->head;
            while (current)
            {
                QueueNode *tmp = current;
                current = current->nextNode;

                if (tmp->age >= algorithmInfo->ageLimit)
                {
                    if (tmp->indexReadyQueue > 0)
                    {
                        tmp->age = 0;
                        tmp->execTime = 0;
                        tmp->indexReadyQueue--;

                        // Check if the new queue algorithm is RR set the RR timer
                        if (scheduler->readyQueueAlgorithms[tmp->indexReadyQueue]->type == RR)
                        {
                            tmp->RRTimer = 0;
                        }
                        else
                        {
                            tmp->RRTimer = -1;
                        }
                        
                        tmp = removeElement(queue, tmp);
                        insertInQueue(scheduler->readyQueue[tmp->indexReadyQueue], tmp);
                    }
                }
            }
        }
    }
}


static void checkEventsInRunningQueue(Computer *computer, Workload *workload, AllStats *stats) 
{
    if (!computer) 
    {
        return; // Exit if computer is NULL
    }

    Scheduler *scheduler = computer->scheduler;
    CPU *cpu = computer->cpu;

    // Iterate through the processes in the running queue
    QueueNode *current = scheduler->runningQueue->head;
    while (current) 
    {
        // Get scheduling algorithm information for the current process
        ProcessStats *processStat = getProcessStats(stats, current->data->pid);
        SchedulingAlgorithm *algorithmInfo = scheduler->readyQueueAlgorithms[current->indexReadyQueue];
        QueueNode *tmp = current;
        current = current->nextNode;

        // Check if the RR timer is finished or the execution time limit is reached
        if (tmp->RRTimer >= algorithmInfo->RRSliceLimit || tmp->execTime >= algorithmInfo->executiontTimeLimit) 
        {
            // Reset RR timer if it's reached and the scheduling algorithm is RR
            if (tmp->RRTimer >= algorithmInfo->RRSliceLimit && algorithmInfo->type == RR) 
            {
                tmp->RRTimer = 0;

                // Perform context switch and set process state to ready if next queue is not empty
                if (scheduler->readyQueue[tmp->indexReadyQueue]->nbrOfNode > 0) 
                {
                    Core *core = findProcessCore(cpu, tmp->data->pid);
                    if (core)
                    {
                        // Set the context switch out of the core
                        core->timer = SWITCH_OUT_DURATION;
                        // Set the state of the core to context switching out
                        core->state = CONTEXT_SWITCHING_OUT;
                        setProcessState(workload, tmp->data->pid, READY);
                        processStat->nbContextSwitches++;
                        continue;
                    }
                }
            }
            
            if (tmp->execTime >= algorithmInfo->executiontTimeLimit) 
            {
                // Reset process parameters and move to the next queue if execution time limit is reached
                if (tmp->indexReadyQueue < scheduler->readyQueueCount - 1) 
                {
                    tmp->indexReadyQueue++;
                    tmp->age = 0;
                    tmp->execTime = 0;

                    // Reset RR timer if the next queue uses RR scheduling
                    if (scheduler->readyQueueAlgorithms[tmp->indexReadyQueue]->type == RR) 
                    {
                        tmp->RRTimer = 0;
                    } 
                    else 
                    {
                        tmp->RRTimer = -1;
                    }

                    // Perform context switch and set process state to ready if next queue is not empty
                    if (scheduler->readyQueue[tmp->indexReadyQueue]->nbrOfNode > 0) 
                    {
                        Core *core = findProcessCore(cpu, tmp->data->pid);
                        if (core)
                        {
                            // Set the context switch out of the core
                            core->timer = SWITCH_OUT_DURATION;
                            // Set the state of the core to context switching out
                            core->state = CONTEXT_SWITCHING_OUT;
                            setProcessState(workload, tmp->data->pid, READY);
                            processStat->nbContextSwitches++;
                            continue;
                        }
                    }
                }
            }
        }
    }
}

/* ---------------- setProcessToCore static function ---------------- */

static int scheduling(Scheduler *scheduler, Workload *workload) 
{
    // Check if the scheduler exists
    if (!scheduler) 
    {
        fprintf(stderr, "Error: The scheduler does not exist.\n");
        return -1;
    }

    // Check if all ready queues are empty
    int allQueuesEmpty = 1;
    for (int i = 0; i < scheduler->readyQueueCount; i++) 
    {
        if (scheduler->readyQueue[i]->nbrOfNode > 0) 
        {
            // At least one ready queue is not empty
            allQueuesEmpty = 0; 
            break;
        }
    }
    if (allQueuesEmpty) 
    {
        // All ready queues are empty
        return -1; 
    }

    // Return the PID according to the algorithm of the first non-empty queue
    for (int i = 0; i < scheduler->readyQueueCount; i++) 
    {
        Queue *queue = scheduler->readyQueue[i];
        SchedulingAlgorithm *algorithmInfo = scheduler->readyQueueAlgorithms[i];
        if (queue->nbrOfNode > 0) {
            switch (algorithmInfo->type) 
            {
                case FCFS:
                    return fcfsAlgorithm(queue);

                case RR:
                    return rrAlgorithm(queue);

                case PRIORITY:
                    return priorityAlgorithm(queue);

                case SJF:
                    return sjfAlgorithm(queue, workload);

                default:
                    break;
            }
        }
    }
    return -1; // No process to execute
}


static int fcfsAlgorithm(Queue *queue)
{
    // Check if the queue is empty or not allocated
    if (!queue || queue->nbrOfNode <= 0)
    {
        return -1;
    }
    
    return queue->head->data->pid;
}


static int rrAlgorithm(Queue *queue)
{
    // Check if the queue is empty or not allocated
    if (!queue || queue->nbrOfNode <= 0)
    {
        return -1;
    }

    return queue->head->data->pid;
}


static int sjfAlgorithm(Queue *queue, Workload *workload)
{
    // Check if the queue is empty or not allocated
    if (!queue || queue->nbrOfNode <= 0)
    {
        return -1;
    }

    QueueNode *current = queue->head;
    int shortestJobPid = -1;
    // Initialize the shortest time with the maximum value of an integer
    int shortestTimeLeft = INT_MAX; 

    // Traverse the queue until reaching the end
    while (current) 
    {
        int timeLeftCurrentProcess = getProcessCurEventTimeLeft(workload, current->data->pid);

        // Compare the remaining time of the current process with the shortest time found so far
        if (timeLeftCurrentProcess < shortestTimeLeft)
        {
            shortestTimeLeft = timeLeftCurrentProcess; // Update the shortest time
            shortestJobPid = current->data->pid; // Update the PID of the shortest job
        }

        current = current->nextNode; // Move to the next node in the queue
    }

    // Return the PID of the job with the shortest remaining time
    return shortestJobPid;
}


static int priorityAlgorithm(Queue *queue)
{
    // Check if the queue is empty or not allocated
    if (!queue || queue->nbrOfNode <= 0)
    {
        return -1;
    }

    QueueNode *current = queue->head;
    int highestPriorityPid = current->data->pid;
    int highestPriorityValue = current->data->priority;

    while (current != NULL)
    {
        // Compare the priority of the current process with the highest priority found so far
        if (current->data->priority < highestPriorityValue)
        {
            highestPriorityValue = current->data->priority;
            highestPriorityPid = current->data->pid;
        }

        current = current->nextNode; // Move to the next node in the queue
    }

    // Return the PID of the process with the highest priority (lowest priority value)
    return highestPriorityPid;
}

/* ---------------- static Init/free Queue functions  --------------- */

static Queue *initQueue(int indexQueue)
{
    // Allocation of the memory for the queue
    Queue *queue = malloc(sizeof(Queue));
    if(!queue)
    {
        return NULL;
    }

    // Initialize the field of the queue
    queue->head = NULL;
    queue->tail = NULL;
    queue->nbrOfNode = 0;
    queue->index = indexQueue;

    return queue;
}


static QueueNode *initQueueNode(PCB *pcb, int indexReadyQueue)
{
    QueueNode *queueNode = malloc(sizeof(QueueNode));
    if (!queueNode)
    {
        return NULL;
    }

    queueNode->data = pcb;
    queueNode->age = 0;
    queueNode->RRTimer = -1;
    queueNode->execTime = 0;
    queueNode->indexReadyQueue = indexReadyQueue;
    queueNode->nextNode = NULL;
    queueNode->prevNode = NULL;

    return queueNode;
}


static void freeQueue(Queue *queue)
{
    if(!queue)
    {
        return;
    }
    
    QueueNode *current = queue->head;
    while (current)
    {
        QueueNode *tmp = current;
        current = current->nextNode;
        free(tmp);
    }

    free(queue);
}

/* ---------------- static Queue functions  ------------------------- */

static int insertInQueue(Queue *queue, QueueNode *node)
{
    // Check if queue and node are not NULL
    if (!queue || !node)
    {
        return 0;
    }

    // If the queue is empty, the new node becomes both the head and the tail
    if (queue->head == NULL)
    {
        queue->head = node;
        queue->tail = node;
    }
    // If the queue is not empty, the new node is inserted at the end of the queue
    else
    {
        queue->tail->nextNode = node;
        node->prevNode = queue->tail;
        queue->tail = node;
    }

    // Update the number of node in the queue
    queue->nbrOfNode++;

    return 1;
}


static QueueNode *removeElement(Queue *queue, QueueNode *node)
{
    // Check if queue and node are not NULL
    if (!queue || !node)
    {
        return NULL;
    }

    // Check if the node to remove is in the queue
    if (!searchInQueue(queue, node->data->pid))
    {
        return NULL;
    }

    // Check if the node is the head of the queue
    if (node == queue->head)
    {
        queue->head = node->nextNode;
        if (queue->head)// Check if the queue is not empty after removing the head
        {
            queue->head->prevNode = NULL;
        }
        else // The queue is now empty, set the tail to NULL
        {
            queue->tail = NULL;
        }
    }
    // Check if the node is the tail of the queue
    else if (node == queue->tail)
    {
        queue->tail = node->prevNode;
        if (queue->tail) // Check if the queue is not empty after removing the tail
        {
            queue->tail->nextNode = NULL;
        }
        else // The queue is now empty, set the head to NULL
        {
            queue->head = NULL;
        }
    }
    else
    {
        node->prevNode->nextNode = node->nextNode;
        node->nextNode->prevNode = node->prevNode;
    }

    // Set the nextNode and prevNode of the removed node to NULL
    node->nextNode = NULL;
    node->prevNode = NULL;

    // Update the number of node in the queue
    queue->nbrOfNode--;

    return node;
}


static QueueNode *searchInQueue(Queue *queue, int pid)
{
    // Check if queue is NULL
    if (!queue)
    {
        return NULL;
    }

    QueueNode *current = queue->head; // Start from the head of the queue

    // Traverse the queue until the end or the PID is found
    while (current != NULL)
    {
        if (current->data->pid == pid)
        {
            return current; // Return the node if the PID is found
        }
        current = current->nextNode; // Move to the next node in the queue
    }

    return NULL; // Return NULL if the PID is not found in the queue
}

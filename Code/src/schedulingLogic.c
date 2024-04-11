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
    PCB *data; // PID of the process in the node
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

static bool isInScheduler(Scheduler *scheduler, int pid);

/* ---------------- schedulingEvents static function ---------------- */



/* ------------------------------------------------------------------ */

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

static int insertInQueue(Queue *queue, QueueNode *node);

static QueueNode *searchInQueue(Queue *queue, int pid);

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

void schedulingEvents(Computer *computer, Workload *workload)
{
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exists.\n");
        return;
    }

    Scheduler *scheduler = computer->scheduler;

    // Check for events in ready queue
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
    
    // Check for events in running queue
    QueueNode *current = scheduler->runningQueue->head;
    while (current)
    {
        SchedulingAlgorithm *algorithmInfo = scheduler->readyQueueAlgorithms[current->indexReadyQueue];
        QueueNode *tmp = current;
        current = current->nextNode;

        // Check if the current process in the running queue is in a RR queue
        if (algorithmInfo->type == RR)
        {
            // Check if the RR timer is finished
            if (tmp->RRTimer >= algorithmInfo->RRSliceLimit)
            {
                Core *core = NULL;
                // Search the core on which the process run
                for (int i = 0; i < computer->cpu->coreCount; i++)
                {
                    if (computer->cpu->cores[i]->state == WORKING)
                    {
                        if (tmp->data->pid == computer->cpu->cores[i]->pid)
                        {
                            core = computer->cpu->cores[i];
                        }
                    }
                }
                
                tmp->RRTimer = 0;

                // Check if there is a limit for the queue
                if (algorithmInfo->executiontTimeLimit > 0)
                {
                    // Check if the limit is reached
                    if (tmp->execTime >= algorithmInfo->executiontTimeLimit)
                    {
                        // We check the queue is not the last one
                        if (tmp->indexReadyQueue < scheduler->readyQueueCount - 1)
                        {
                            tmp->indexReadyQueue++;
                            tmp->age = 0;
                            tmp->execTime = 0;

                            // Check if the new queue algorithm is RR set the RR timer
                            if (scheduler->readyQueueAlgorithms[tmp->indexReadyQueue]->type == RR)
                            {
                                tmp->RRTimer = 0;
                            }
                            else
                            {
                                tmp->RRTimer = -1;
                            }
                        }
                    }
                }

                if ((scheduler->readyQueue[tmp->indexReadyQueue]->nbrOfNode > 0))
                {
                    tmp = removeElement(scheduler->runningQueue, tmp);
                    insertInQueue(scheduler->readyQueue[tmp->indexReadyQueue], tmp);

                    setProcessState(workload, tmp->data->pid, READY);

                    // Set the context switch out of the core
                    core->timer = SWITCH_OUT_DURATION;
                    // Set the state of the core to context switching out
                    core->state = CONTEXT_SWITCHING_OUT;
                    //Remove the pid from the core
                    core->pid = -1;
                }
            }
            printf("TEST\n");
            // Check if the exec time limit is reached before the RR timer
            if (tmp->execTime >= algorithmInfo->executiontTimeLimit)
            {
                // We check the queue is not the last one
                if (tmp->indexReadyQueue < scheduler->readyQueueCount - 1)
                {
                    tmp->indexReadyQueue++;
                    tmp->age = 0;
                    tmp->execTime = 0;

                    // Check if the new queue algorithm is RR set the RR timer
                    if (scheduler->readyQueueAlgorithms[tmp->indexReadyQueue]->type == RR)
                    {
                        tmp->RRTimer = 0;
                    }
                    else
                    {
                        tmp->RRTimer = -1;
                    }

                    if (scheduler->readyQueue[tmp->indexReadyQueue]->nbrOfNode > 0)
                    {
                        Core *core = NULL;
                        // Search the core on which the process run
                        for (int i = 0; i < computer->cpu->coreCount; i++)
                        {
                            if (computer->cpu->cores[i]->state == WORKING)
                            {
                                if (tmp->data->pid == computer->cpu->cores[i]->pid)
                                {
                                    core = computer->cpu->cores[i];
                                }
                            }
                        }

                        tmp = removeElement(scheduler->runningQueue, tmp);
                        insertInQueue(scheduler->readyQueue[tmp->indexReadyQueue], tmp);

                        setProcessState(workload, tmp->data->pid, READY);

                        // Set the context switch out of the core
                        core->timer = SWITCH_OUT_DURATION;
                        // Set the state of the core to context switching out
                        core->state = CONTEXT_SWITCHING_OUT;
                        //Remove the pid from the core
                        core->pid = -1;
                    }
                }
            }
        }
        // If the current process in the running queue is in a different queue
        else
        {
            // Check if there is a limit for the queue
            if (algorithmInfo->executiontTimeLimit > 0)
            {
                // Check if the limit is reached
                if (tmp->execTime >= algorithmInfo->executiontTimeLimit)
                {
                    // We check the queue is not the last one
                    if (tmp->indexReadyQueue < scheduler->readyQueueCount - 1)
                    {
                        tmp->indexReadyQueue++;
                        tmp->age = 0;
                        tmp->execTime = 0;

                        // Check if the new queue algorithm is RR set the RR timer
                        if (scheduler->readyQueueAlgorithms[tmp->indexReadyQueue]->type == RR)
                        {
                            tmp->RRTimer = 0;
                        }
                        else
                        {
                            tmp->RRTimer = -1;
                        }
                    }
                }
            }
        }
    }
}



void updateSchedulingValue(Scheduler *scheduler, Workload *workload)
{
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exist.\n");
        return;
    }

    // Update values on ready queues
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        Queue *queue = scheduler->readyQueue[i];
        SchedulingAlgorithm *algoritmInfo = scheduler->readyQueueAlgorithms[i];

        if (algoritmInfo->ageLimit > 0)
        {
            QueueNode *current = queue->head;
            while (current)
            {
                current->age++;
                current = current->nextNode;
            }
        }
    }

    // Update values on running queue
    QueueNode *current = scheduler->runningQueue->head;
    while (current)
    {
        SchedulingAlgorithm *algoritmInfo = scheduler->readyQueueAlgorithms[current->indexReadyQueue];
        if (getProcessState(workload, current->data->pid) == RUNNING)
        {
            if (algoritmInfo->executiontTimeLimit > 0)
            {
                current->execTime++;
            }
            if (algoritmInfo->type == RR)
            {
                current->RRTimer++;
            }
        }
        current = current->nextNode;
    }
}



void preemption(Computer *computer, Workload *workload)
{
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }

    Scheduler *scheduler = computer->scheduler;
    CPU *cpu = computer->cpu;
    
    // Check all process that run on a core to see if a higher priority is available
    for (int i = 0; i < cpu->coreCount; i++)
    {
        Core *core = cpu->cores[i];
        if (core->state == WORKING)
        {
            QueueNode *node = searchInQueue(scheduler->runningQueue, core->pid);
            if (node)
            {
                SchedulingAlgorithm *algorithmInfo = scheduler->readyQueueAlgorithms[node->indexReadyQueue];
                for (int j = 0; j <= node->indexReadyQueue; j++)
                {
                    if (j == node->indexReadyQueue)
                    {
                        if (algorithmInfo->type == SJF)
                        {
                            int pid = node->data->pid;
                            int newPid = sjfAlgorithm(scheduler->readyQueue[node->indexReadyQueue], workload);
                            if (newPid == -1)
                            {
                                break;
                            }
                            

                            if (getProcessCurEventTimeLeft(workload, pid) > getProcessCurEventTimeLeft(workload, newPid))
                            {
                                node = removeElement(scheduler->runningQueue, node);
                                insertInQueue(scheduler->readyQueue[node->indexReadyQueue], node);

                                setProcessState(workload, node->data->pid, READY);

                                // Set the context switch out of the core
                                core->timer = SWITCH_OUT_DURATION;
                                // Set the state of the core to context switching out
                                core->state = CONTEXT_SWITCHING_OUT;
                                //Remove the pid from the core
                                core->pid = -1;
                            }
                        }
                    }
                    // Check if there is a process in higher queue (preempt if so)
                    else if (j < node->indexReadyQueue)
                    {
                        if (scheduler->readyQueue[j]->nbrOfNode > 0)
                        {
                            if (algorithmInfo->type == RR)
                            {   
                                node->RRTimer = 0;
                            }
                            
                            node = removeElement(scheduler->runningQueue, node);
                            insertInQueue(scheduler->readyQueue[node->indexReadyQueue], node);

                            setProcessState(workload, node->data->pid, READY);

                            // Set the context switch out of the core
                            core->timer = SWITCH_OUT_DURATION;
                            // Set the state of the core to context switching out
                            core->state = CONTEXT_SWITCHING_OUT;
                            //Remove the pid from the core
                            core->pid = -1;
                        }
                    }
                }
            }
        }

        return;
    }
}



/* --------------------- Assign Ressources ------------------------ */

int scheduling(Scheduler *scheduler, Workload *workload)
{
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exist.\n");
        return -1;
    }

    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        if (scheduler->readyQueue[i]->nbrOfNode <= 0)
        {
            return -1;
        }
    }

    // return the pid accordingly to the algorithm of the first queue not empty
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        Queue *queue = scheduler->readyQueue[i];
        SchedulingAlgorithm *algorithmInfo = scheduler->readyQueueAlgorithms[i];
        if (queue->nbrOfNode > 0)
        {
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
    return -1;
}

void setProcessToCore(Computer *computer, int indexCore, int pid)
{
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }

    printf("%d\n", pid);
    
    if (pid == -1)
    {
        return;
    }
    
    Scheduler *scheduler = computer->scheduler;
    Core *core = computer->cpu->cores[indexCore];

    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        QueueNode *node = searchInQueue(scheduler->readyQueue[i], pid);
        if (node)
        {
            if (core->state == IDLE)
            {
                node = removeElement(scheduler->readyQueue[i], node);
                insertInQueue(scheduler->runningQueue, node);

                core->pid = pid;
                core->state = CONTEXT_SWITCHING_IN;
                core->timer = SWITCH_IN_DURATION;
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



void printQueue(Scheduler *scheduler)
{
    printf("\n|------------------QUEUE-----------------------|\n");
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        printf("Queue %d: head---> ", i);
        QueueNode *current = scheduler->readyQueue[i]->head;
        while (current)
        {
            printf("%d ", current->data->pid);
            current = current->nextNode;
        }
        printf("<---tail\n");

        printf("Number of nodes: %d\n", scheduler->readyQueue[i]->nbrOfNode);
    }
    printf("|--------------WAITING QUEUE-------------------|\n");
        printf("Waiting queue: head---> ");
        QueueNode *current = scheduler->waitingQueue->head;
        while (current)
        {
            printf("%d ", current->data->pid);
            current = current->nextNode;
        }
        printf("<---tail\n");
    printf("|--------------RUNNING QUEUE-------------------|\n");
        printf("Running queue: head---> ");
        current = scheduler->runningQueue->head;
        while (current)
        {
            printf("%d ", current->data->pid);
            current = current->nextNode;
        }
        printf("<---tail\n");
    printf("|----------------------------------------------|\n\n");
}







/* ---------------------------- static functions --------------------------- */

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
        if (queue->head) // Check if the queue is not empty after removing the head
            queue->head->prevNode = NULL;
        else // The queue is now empty, set the tail to NULL
            queue->tail = NULL;
    }
    // Check if the node is the tail of the queue
    else if (node == queue->tail)
    {
        queue->tail = node->prevNode;
        if (queue->tail) // Check if the queue is not empty after removing the tail
            queue->tail->nextNode = NULL;
        else // The queue is now empty, set the head to NULL
            queue->head = NULL;
    }
    else
    {
        node->prevNode->nextNode = node->nextNode;
        node->nextNode->prevNode = node->prevNode;
    }

    // Set the nextNode and prevNode of the removed node to NULL
    node->nextNode = NULL;
    node->prevNode = NULL;

    queue->nbrOfNode--;

    return node;
}

static QueueNode *searchInQueue(Queue *queue, int pid)
{
    if (!queue)
    {
        return NULL;
    }
    
    QueueNode *current = queue->head;

    while (current)
    {
        if (current->data->pid == pid)
        {
            return current;
        }
        current = current->nextNode;
    }

    return NULL;
}
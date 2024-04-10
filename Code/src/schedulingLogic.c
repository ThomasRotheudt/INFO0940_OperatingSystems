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
 * These queues contain the PID of the processes.
 */
typedef struct Queue_t Queue;

/**
 * The QueueNode struct represents a element of the scheduler's queues, here it is a process's PID. 
 * It contains a pointer to the next element of the queue, and a data field which is the pid
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

static bool schedulerIsEmpty(Scheduler *scheduler);

static bool isInScheduler(Scheduler *scheduler, int pid);

static int fcfsAlgorithm(Queue *queue);

static int rrAlgorithm(Queue *queue);

static int sjfAlgorithm(Queue *queue);

static int priorityAlgorithm(Queue *queue);

/* ---------------- static Init/free Queue functions  --------------- */

static Queue *initQueue(int indexQueue);

static QueueNode *initQueueNode(PCB *pcb, int indexReadyQueue, int age, int execTime);

static void freeQueue(Queue *queue);

/* ---------------- static Queue functions  ------------------------- */

static int insertInQueue(Queue *queue, QueueNode *node);

static QueueNode *searchInQueue(Queue *queue, int pid);

static QueueNode *removeElement(Queue *queue, QueueNode *node);

static bool queueIsEmpty(Queue *queue);

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

    if (!isInScheduler(scheduler, data->pid))
    {
        QueueNode *newNode = initQueueNode(data, FIRST_QUEUE, 0, 0);
        if (!newNode)
        {
            fprintf(stderr, "Error: The new node can't be created.\n");
            return;
        }

        // Check if the first queue algorithm is RR to set the RR timer
        if (scheduler->readyQueueAlgorithms[FIRST_QUEUE]->type == RR)
        {
            newNode->RRTimer = 0;
        }

        insertInQueue(scheduler->readyQueue[newNode->indexReadyQueue], newNode);
    }
}

void addProcessToWaitingQueue(Scheduler *scheduler, int pid)
{
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exist.\n");
        return;
    }

    QueueNode *node = searchInQueue(scheduler->runningQueue, pid);
    if (node)
    {
        node = removeElement(scheduler->runningQueue, node);
        insertInQueue(scheduler->waitingQueue, node);
    }
}

void returnFromWaitQueue(Scheduler *scheduler, int pid)
{
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exist.\n");
        return;
    }

    QueueNode *node = searchInQueue(scheduler->waitingQueue, pid);
    if (node)
    {
        node = removeElement(scheduler->waitingQueue, node);
        insertInQueue(scheduler->readyQueue[node->indexReadyQueue], node);
    }

    return;
}

void removeProcessFromScheduler(Computer *computer, int pid, int indexCore)
{
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }

    Scheduler *scheduler = computer->scheduler;
    CPU *cpu = computer->cpu;

    QueueNode *node = searchInQueue(scheduler->runningQueue, pid);
    if (node)
    {
        node = removeElement(scheduler->runningQueue, node);
        free(node);
    }

    Core *core = cpu->cores[indexCore];
    core->pid = -1;
    core->state = IDLE;
}

/* -------------------- Event & Update values --------------------- */

void schedulingEvents(Scheduler *scheduler, Computer *computer, Workload *workload)
{
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exists.\n");
        return;
    }

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
                printf("PID: %d --> age = %d   index queue: %d\n", current->data->pid, current->age, current->indexReadyQueue);
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

        printf("PID: %d Exec Time: %d RR timer: %d, RR Slice Limit: %d, Index Queue: %d\n", tmp->data->pid, tmp->execTime, tmp->RRTimer, algorithmInfo->RRSliceLimit, tmp->indexReadyQueue);
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

                if (!queueIsEmpty(scheduler->readyQueue[tmp->indexReadyQueue]))
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

                    if (!queueIsEmpty(scheduler->readyQueue[tmp->indexReadyQueue]))
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
                for (int j = 0; j <= node->indexReadyQueue; j++)
                {
                    if (j < node->indexReadyQueue)
                    {
                        if (!queueIsEmpty(scheduler->readyQueue[j]))
                        {
                            if (scheduler->readyQueueAlgorithms[node->indexReadyQueue]->type == RR)
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
    }
}
/* --------------------- Assign Ressources ------------------------ */

int scheduling(Scheduler *scheduler)
{
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exist.\n");
        return -1;
    }

    if (schedulerIsEmpty(scheduler))
    {
        return -1;
    }

    // return the pid accordingly to the algorithm of the first queue not empty
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        Queue *queue = scheduler->readyQueue[i];
        SchedulingAlgorithm *algorithmInfo = scheduler->readyQueueAlgorithms[i];
        if (!queueIsEmpty(queue))
        {
            switch (algorithmInfo->type)
            {
            case FCFS:
                return fcfsAlgorithm(queue);

            case RR:
                return rrAlgorithm(queue);

            case PRIORITY:
                return -1;

            case SJF:
                return -1;

            default:
                break;
            }
        }
    }
}

void setProcessToCore(Computer *computer, int indexCore, int pid)
{
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }
    
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

    if (!queueIsEmpty(scheduler->waitingQueue))
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

static bool schedulerIsEmpty(Scheduler *scheduler)
{
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        if (!queueIsEmpty(scheduler->readyQueue[i]))
        {
            return false;
        }
    }

    return true;
}

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
    if (!queue)
    {
        return;
    }
    
    return queue->head->data->pid;
}

static int rrAlgorithm(Queue *queue)
{
    if (!queue)
    {
        return;
    }

    return queue->head->data->pid;
}

static int sjfAlgorithm(Queue *queue)
{

}

static int priorityAlgorithm(Queue *queue)
{

}

/* ---------------- static Init/free Queue functions  --------------- */

static Queue *initQueue(int indexQueue)
{
    Queue *queue = malloc(sizeof(Queue));
    if(!queue)
    {
        return NULL;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->nbrOfNode = 0;
    queue->index = indexQueue;

    return queue;
}

static QueueNode *initQueueNode(PCB *pcb, int indexReadyQueue, int age, int execTime)
{
    QueueNode *queueNode = malloc(sizeof(QueueNode));
    if (!queueNode)
    {
        return NULL;
    }

    queueNode->data = pcb;
    queueNode->age = age;
    queueNode->RRTimer = -1;
    queueNode->execTime = execTime;
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

static bool queueIsEmpty(Queue *queue)
{
    // Check if queue is not NULL
    if (!queue)
    {
        return false;
    }

    // If the head of the queue is NULL, the queue is empty
    if (queue->head == NULL)
    {
        return true;
    }

    // If the head of the queue is not NULL, the queue is not empty
    return false;
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
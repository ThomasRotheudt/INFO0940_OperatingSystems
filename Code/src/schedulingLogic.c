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
    int limit; // The execution time limit on the queue
    QueueNode *nextNode;
};

struct Queue_t
{
    int index;
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

static bool isInScheduler(Scheduler *scheduler, int pid);

/**
 * Move the process in the node from its current queue to the next one. It stays in its queue if it is the last queue of the scheduler.
 * 
 * @param scheduler: the scheduler
 * @param node: the node to move
 */
static void moveProcessToNextQueue(Scheduler *scheduler, QueueNode *node);

/**
 * Move the process in the node from its current queue to the previous one. It stays in its queue if it is the first queue of the scheduler.
 * 
 * @param scheduler 
 * @param node 
 */
static void moveProcessToPreviousQueue(Scheduler *scheduler, QueueNode *node);

static void moveProcessToRunningQueue(Scheduler *scheduler, QueueNode *node);

static void moveProcessToWaitingQueue(Scheduler *scheduler, QueueNode *node);

/* ---------------- static Queue functions  --------------- */
static Queue *initQueue(int indexQueue);

static void freeQueue(Queue *queue);

static QueueNode *initQueueNode(PCB *data, int indexQueue, int age, int executionTime);

/**
 * Add a new element to the tail of a ready queue
 * 
 * @param queue: the queue
 * @param data: the PCB of a process
 * @return 1 if the new PCB is enqueued, 0 if an error occurs
 */
static int enqueue(Queue *queue, PCB *data, int age, int executionTime, int indexReadyQueue);

/**
 * Remove a specific process given its PID
 * 
 * @param queue: the queue
 * @param pid: the pid of the process
 * @return A PCB pointer to the removed element
 */
static PCB *removeElement(Queue *queue, int pid);

/**
 * Check if the queue is empty
 * 
 * @param queue: the queue 
 * @return true 
 * @return false 
 */
static bool isEmpty(Queue *queue);

/**
 * Check if a process is in the queue given its PID
 * 
 * @param queue: the queue
 * @param pid: the pid of the process
 * @return true 
 * @return false 
 */
static bool isInQueue(Queue *queue, int pid);
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
    scheduler->runningQueue = initQueue(0);
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
    //! hardcode de l'index de la wait queue si jamais besoin de plusieurs voir ready queue
    scheduler->waitingQueue = initQueue(0);
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

void addProcessToScheduler(Scheduler *scheduler, PCB *data)
{
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exists");
        return;
    }

    if(!isInScheduler(scheduler, data->pid))
    {
        if (!enqueue(scheduler->readyQueue[FIRST_QUEUE], data, 0, 0, FIRST_QUEUE))
        {
            fprintf(stderr, "Error: The process cannot be added to the scheduler.");
            return;
        }
    }
    return;
}

void testScheduling(Scheduler *scheduler)
{
    
}

void schedulingEvents(Scheduler *scheduler)
{
    if (!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exists.\n");
        return;
    }

    // Check the limits of each process
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        int ageLimit = scheduler->readyQueueAlgorithms[i]->ageLimit;
        int executiontTimeLimit = scheduler->readyQueueAlgorithms[i]->executiontTimeLimit;

        // If at least one of the limit exist (is greater than 0 and NO_LIMIT), we check the queue.
        if ((ageLimit > 0 || executiontTimeLimit > 0) && (!isEmpty(scheduler->readyQueue[i])))
        {
            // Check every process of the queue
            QueueNode *current = scheduler->readyQueue[i]->head;
            while (current)
            {
                // If the age of the process is greater than the limit move the process to the previous queue
                if (current->age >= ageLimit)
                {
                    moveProcessToPreviousQueue(scheduler, current);
                }
                // If the execution time of the process is greater than the limit move the process to the next queue
                else if (current->limit >= executiontTimeLimit)
                {
                    moveProcessToNextQueue(scheduler, current);
                }

                current = current->nextNode;
            }
        }
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
    if(!scheduler)
    {
        fprintf(stderr, "Error: The scheduler does not exists");
        return false;
    }

    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        if(isInQueue(scheduler->readyQueue[i], pid))
        {
            return true;
        }
    }

    if(isInQueue(scheduler->waitingQueue, pid))
    {
        return true;
    }
    
    return false;
}

static void moveProcessToNextQueue(Scheduler *scheduler, QueueNode *node)
{
    int indexNextQueue = node->indexReadyQueue++;
    if(indexNextQueue >= scheduler->readyQueueCount - 1)
    {
        return;
    }

    PCB *data = removeElement(scheduler->readyQueue[node->indexReadyQueue], node->data->pid);

    enqueue(scheduler->readyQueue[indexNextQueue], data, 0, 0, indexNextQueue);
}

static void moveProcessToPreviousQueue(Scheduler *scheduler, QueueNode *node)
{
    int indexPreviousQueue = node->indexReadyQueue - 1;
    if (indexPreviousQueue <= 0)
    {
        return;
    }
    
    PCB *data = removeElement(scheduler->readyQueue[node->indexReadyQueue], node->data->pid);

    enqueue(scheduler->readyQueue[indexPreviousQueue], data, 0, 0, indexPreviousQueue);
}
/* 
static void moveProcessToRunningQueue(Scheduler *scheduler, QueueNode *node)
{

}

static void moveProcessToWaitingQueue(Scheduler *scheduler, QueueNode *node)
{

} */
/* ---------------- static Queue functions  --------------- */

static Queue *initQueue(int indexQueue)
{
    Queue *queue = malloc(sizeof(Queue));
    if(!queue)
    {
        return NULL;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->index = indexQueue;

    return queue;
}

static QueueNode *initQueueNode(PCB *data, int indexQueue, int age, int executionTime)
{
    QueueNode *queueNode = malloc(sizeof(QueueNode));
    if(!queueNode)
    {
        return NULL;
    }

    queueNode->data = data;
    queueNode->age = age;
    queueNode->limit = executionTime;
    queueNode->indexReadyQueue = indexQueue;
    queueNode->nextNode = NULL;

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

static int enqueue(Queue *queue, PCB *data, int age, int executionTime, int indexReadyQueue)
{
    if(!queue)
    {
        return 0;
    }

    QueueNode *queueNode = initQueueNode(data, indexReadyQueue, age, executionTime);
    if(!queueNode)
    {
        return 0;
    }

    if(!queue->head)
    {
        queue->head = queueNode;
        queue->tail = queueNode;
    }
    else
    {
        queue->tail->nextNode = queueNode;
        queue->tail = queueNode;
    }

    return 1;
}

static PCB *removeElement(Queue *queue, int pid)
{
    if (!queue)
    {
        fprintf(stderr, "Error: The queue does not exists.");
        return NULL;
    }

    QueueNode* current = queue->head;
    QueueNode* previous = NULL;

    while (current) 
    {
        if (current->data->pid == pid) 
        {
            if (!previous) 
            {
                queue->head = queue->head->nextNode;
                if (queue->head == NULL) 
                {
                    queue->tail = NULL;
                }
            } 
            else 
            {
                previous->nextNode = current->nextNode;
                if (current == queue->tail) 
                {
                    queue->tail = previous;
                }
            }

            PCB *data = current->data;
            free(current);
            return data;
        }

        previous = current;
        current = current->nextNode;
    }
    
    return NULL;
}

static bool isEmpty(Queue *queue)
{
    return (!queue->head && !queue->tail) ? true : false;
}

static bool isInQueue(Queue *queue, int pid)
{
    if(!queue)
    {
        fprintf(stderr, "Error: the queue does not exist.");
        return false;
    }

    QueueNode *current = queue->head;
    while (current)
    {
        if (current->data->pid == pid)
        {
            return true;
        }
        current = current->nextNode;
    }
    
    return false;
}


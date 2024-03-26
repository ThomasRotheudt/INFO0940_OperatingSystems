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
    int data; // PID of the process in the node
    int indexQueue; // The index of the queue in which the node is (use it when returns from waiting queue) 
    int age; // The time the process has spent in its current queue
    QueueNode *nextNode;
};

struct Queue_t
{
    QueueNode *tail;
    QueueNode *head;
};

struct Scheduler_t
{
    // This is not the ready queues, but the ready queue algorithms
    SchedulingAlgorithm **readyQueueAlgorithms;
    Queue **readyQueue;
    Queue *waitingQueue;
    int readyQueueCount;
};

/* ---------------------------- static functions --------------------------- */

/* ---------------- static Queue functions  --------------- */
static Queue *initQueue();

static void freeQueue(Queue *queue);

/**
 * checks if the data is already present in the queue
 *
 * @param queue: the queue
 * @param data: the data to check
 *
 * @return true if the data is in the queue, false otherwise
 */
static bool isInQueue(Queue *queue, int data);

/**
 * insert a new data in the queue
 *
 * @param queue: the queue
 * @param data: the data to store in the queue
 *
 * @return true if the data is correctly stored, false otherwise
 */
static bool enqueue(Queue *queue, int data);

/**
 * remove the head data in the queue
 *
 * @param queue: the queue
 * @param data: the data to dequeue
 *
 * @return return the data, -1 if an error occurs
 */
static int dequeue(Queue *queue);

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
    scheduler->readyQueue = malloc(readyQueueCount * sizeof(Queue));
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

    // Initialisation of queues
    for (int i = 0; i < readyQueueCount; i++)
    {
        scheduler->readyQueue[i] = initQueue();
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

    //Init of the wait queue
    scheduler->waitingQueue = initQueue();
    if(!scheduler->waitingQueue)
    {
        // Freeing ready queue algorithms
        for (int i = 0; i < scheduler->readyQueueCount; i++)
        {
            free(scheduler->readyQueueAlgorithms[i]);
        }
        free(scheduler->readyQueueAlgorithms);
        
        // Freeing ready queues
        for (int i = 0; i < scheduler->readyQueueCount; i++)
        {
            freeQueue(scheduler->readyQueue[i]);
        }
        free(scheduler->readyQueue);

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

    freeQueue(scheduler->waitingQueue);
    free(scheduler);
}

/* -------------------------- scheduling functions ------------------------- */

void addProcessToScheduler(Scheduler *scheduler, int pid)
{
    if(!scheduler)
    {
        return;
    }
    Queue *firstQueue = scheduler->readyQueue[FIRST_QUEUE];

    if (!isInQueue(firstQueue, pid))
    {
        enqueue(firstQueue, pid);
    }
}


void printQueue(Scheduler *scheduler)
{
    printf("\n|--------------------QUEUE---------------------|\n");
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        printf("Queue %d: head---> ", i);
        QueueNode *current = scheduler->readyQueue[i]->head;
        while (current)
        {
            printf("%d ", current->data);
            current = current->nextNode;
        }
        printf("<---tail\n");
    }
    printf("|----------------------------------------------|\n");
}

/* ---------------------------- static functions --------------------------- */





/* ---------------- static Queue functions  --------------- */

static Queue *initQueue()
{
    Queue *queue = malloc(sizeof(Queue));
    if(!queue)
    {
        return NULL;
    }

    queue->head = NULL;
    queue->tail = NULL;

    return queue;
}

static QueueNode *initQueueNode(int data)
{
    QueueNode *queueNode = malloc(sizeof(queueNode));
    if(!queueNode)
    {
        return NULL;
    }

    queueNode->data = data;
    queueNode->age = 0;
    queueNode->indexQueue = FIRST_QUEUE;
    queueNode->nextNode = NULL;

    return queueNode;
}

static bool isInQueue(Queue *queue, int data)
{
    if(!queue)
    {
        return false;
    }

    QueueNode *current = queue->head;
    while (current)
    {
        if (current->data == data)
        { 
            return true;
        }

        current = current->nextNode;
    }
    
    return false;
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

static bool enqueue(Queue *queue, int data)
{
    if(!queue)
    {
        return false;
    }

    QueueNode *queueNode = initQueueNode(data);
    if(!queueNode)
    {
        return false;
    }

    if(!queue->head)
    {
        printf("empty");
        queue->head = queueNode;
        queue->tail = queueNode;
    }
    else
    {
        queue->tail->nextNode = queueNode;
        queue->tail = queueNode;
    }

    return true;
}

static int dequeue(Queue *queue)
{
    if(!queue || !queue->head)
    {
        return -1;
    }

    int data = queue->head->data;

    QueueNode *tmp = queue->head;
    queue->head = tmp->nextNode;

    // If the head is NULL the queue is empty
    if (queue->head == NULL)
    {
        queue->tail = NULL;
    }

    free(tmp);

    return data;
}




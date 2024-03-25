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
    int data; //PID of the process in the node
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
    Queue *readyQueue;
    Queue *waitingQueue;
    int readyQueueCount;

};

/* ---------------------------- static functions --------------------------- */

/* ---------------- static Queue functions  --------------- */
static Queue *initQueue();

static void freeQueue(Queue *queue);

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

    scheduler->readyQueue = initQueue();
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

    scheduler->waitingQueue = initQueue();
    if(!scheduler->waitingQueue)
    {
        for (int i = 0; i < scheduler->readyQueueCount; i++)
        {
            free(scheduler->readyQueueAlgorithms[i]);
        }
        free(scheduler->readyQueueAlgorithms);
        freeQueue(scheduler->readyQueue);
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
    freeQueue(scheduler->readyQueue);
    freeQueue(scheduler->waitingQueue);
    free(scheduler);
}

/* -------------------------- scheduling functions ------------------------- */

bool addProccess(Scheduler *scheduler, int pid)
{
    if(!scheduler)
    {
        return false;
    }

    enqueue(scheduler->readyQueue, pid);
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
    queueNode->nextNode = NULL;

    return queueNode;
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


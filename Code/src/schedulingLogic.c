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
    QueueNode *nextNode;
    QueueNode *prevNode;
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
 * Moves a process from the waiting queue to a ready queue.
 *
 * @param scheduler The scheduler
 * @param node The node containing the process to move.
 */
static void moveProcessFromWaitingQueue(Scheduler *scheduler, QueueNode *node);

/**
 * Moves a process from the running queue to the waiting queue.
 *
 * @param scheduler The scheduler.
 * @param node The node containing the process to move.
 */
static void moveProcessFromRunningQueue(Scheduler *scheduler, QueueNode *node);

/**
 * Moves a process from one ready queue to another, or to the running or waiting queue.
 *
 * @param scheduler The scheduler
 * @param node The node containing the process to move.
 * @param newQueue The destination queue.
 */
static void moveProcessFromReadyQueue(Scheduler *scheduler, QueueNode *node, Queue *newQueue);

/* ---------------- static Init/free Queue functions  --------------- */
static Queue *initQueue(int indexQueue);

static QueueNode *initQueueNode(PCB *pcb, int indexReadyQueue, int age, int execTime);

static void freeQueue(Queue *queue);

static QueueNode *initQueueNode(PCB *pcb, int indexReadyQueue, int age, int execTime);

/* ---------------- static Queue functions  ------------------------- */

static int insertInQueue(Queue *queue, QueueNode *node);

static QueueNode *removeElement(Queue *queue, QueueNode *node);

static bool isInQueue(Queue *queue, int pid);

static bool isEmpty(Queue *queue);

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

void addProcessToScheduler(Scheduler *scheduler, PCB *data)
{
    if(!isInScheduler(scheduler, data->pid))
    {
        QueueNode *node = initQueueNode(data, FIRST_QUEUE, 0, 0);
        if (!node)
        {
            fprintf(stderr, "Error: The new node can't be created.\n");
            return;
        }

        if (!insertInQueue(scheduler->readyQueue[FIRST_QUEUE], node))
        {
            fprintf(stderr, "Error: Can't add the process to the scheduler");
            return;
        }   
    }
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
        int executionTimeLimit = scheduler->readyQueueAlgorithms[i]->executiontTimeLimit;

        // If at least one of the limit exist (is greater than 0 and NO_LIMIT), we check the queue.
        if ((ageLimit > 0 || executionTimeLimit > 0) && (!isEmpty(scheduler->readyQueue[i])))
        {
            // Check every process of the queue
            QueueNode *current = scheduler->readyQueue[i]->head;
            while (current)
            {
                // If the age of the process is greater than the limit move the process to the previous queue
                if (current->age >= ageLimit)
                {
                    moveProcessFromReadyQueue(scheduler, current, scheduler->readyQueue[current->indexReadyQueue - 1]);
                }
                // If the execution time of the process is greater than the limit move the process to the next queue
                else if (current->execTime >= executionTimeLimit)
                {
                    moveProcessFromReadyQueue(scheduler, current, scheduler->readyQueue[current->indexReadyQueue + 1]);
                }

                //Update the age timer
                current->age++;

                current = current->nextNode;
            }
        }
    }

    // Update the execution time timer of each running process if there is
    if (!isEmpty(scheduler->runningQueue))
    {
        QueueNode *current = scheduler->runningQueue->head;
        while (current)
        {
            current->execTime++;
            current = current->nextNode;
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

void scheduling(Scheduler *scheduler)
{

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
        if (isInQueue(scheduler->readyQueue[i], pid))
        {
            return true;
        }
    }

    // Check the running queue
    if (isInQueue(scheduler->runningQueue, pid))
    {
        return true;
    }

    // Check the waiting queue
    if (isInQueue(scheduler->waitingQueue, pid))
    {
        return true;
    }

    // If we iterated over all queues without finding a match, return false
    return false;
}

static void moveProcessFromReadyQueue(Scheduler *scheduler, QueueNode *node, Queue *newQueue)
{
    // Return if the destination queue is NULL or is the same as the current queue.
    if (!newQueue || newQueue->index == node->indexReadyQueue)
    {
        return;
    }

    // Check if the process is in one of the ready queues.
    bool isInReadyQueues = false;
    for (int i = 0; i < scheduler->readyQueueCount; i++)
    {
        if (isInQueue(scheduler->readyQueue[i], node->data->pid))
        {
            isInReadyQueues = true;
            break; // We found the process in a ready queue, so we can stop searching.
        }
    }

    // If the process is in a ready queue, remove it and insert it into the destination queue.
    if (isInReadyQueues)
    {
        QueueNode *newNode = removeElement(scheduler->readyQueue[node->indexReadyQueue], node);

        // If the destination queue is the running or waiting queue, simply insert the process.
        if (newQueue == scheduler->runningQueue || newQueue == scheduler->waitingQueue)
        {
            insertInQueue(newQueue, newNode);
        }
        // If the destination queue is another ready queue, update the process's index, age, and execTime.
        else
        {
            newNode->indexReadyQueue = newQueue->index;
            newNode->age = 0;
            newNode->execTime = 0;
            insertInQueue(newQueue, newNode);
        }
    }
}

static void moveProcessFromWaitingQueue(Scheduler *scheduler, QueueNode *node)
{
    if (!node)
    {
        return;
    }

    // Check if the process is in the waiting queue.
    if (isInQueue(scheduler->waitingQueue, node->data->pid))
    {
        QueueNode *newNode = removeElement(scheduler->waitingQueue, node);

        // Insert the process into the ready queue with the same index as before.
        insertInQueue(scheduler->readyQueue[newNode->indexReadyQueue], newNode);
    }
}

static void moveProcessFromRunningQueue(Scheduler *scheduler, QueueNode *node)
{
    if (!node)
    {
        return;
    }

    // Check if the process is in the running queue.
    if (isInQueue(scheduler->runningQueue, node->data->pid))
    {
        QueueNode *newNode = removeElement(scheduler->runningQueue, node);

        // Insert the process into the waiting queue.
        insertInQueue(scheduler->waitingQueue, newNode);
    }
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
    if (!isInQueue(queue, node->data->pid))
    {
        return NULL;
    }

    // Check if the node is the head of the queue
    if (node == queue->head)
    {
        queue->head = node->nextNode;
    }
    // Check if the node is the tail of the queue
    else if (node == queue->tail)
    {
        queue->tail = node->prevNode;
    }
    else
    {
        node->prevNode->nextNode = node->nextNode;
        node->nextNode->prevNode = node->prevNode;
    }

    // Set the nextNode and prevNode of the removed node to NULL
    node->nextNode = NULL;
    node->prevNode = NULL;

    return node;
}

static bool isInQueue(Queue *queue, int pid)
{
    // Check if queue is not NULL
    if (!queue)
    {
        return false;
    }

    // Iterate over all nodes in the queue
    QueueNode *current = queue->head;
    while (current != NULL)
    {
        // Check if the PID of the process in the node matches the given PID
        if (current->data->pid == pid)
        {
            return true;
        }

        current = current->nextNode;
    }

    // If we iterated over all nodes without finding a match, return false
    return false;
}

static bool isEmpty(Queue *queue)
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

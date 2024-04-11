// You should complete the structures of the CPU and Disk in the header file
// and modify their initialization and freeing functions here.
// You should also implement the triggering of an interrupt here (and put its
// declaration in the header file to access it from simulation.c).

#include <stdio.h>
#include <stdlib.h>

#include "computer.h"
#include "schedulingLogic.h"

#define INTERRUPT_TIME 1


/* ---------------------------- static functions --------------------------- */


/* -------------------------- init/free functions -------------------------- */

Computer *initComputer(Scheduler *scheduler, CPU *cpu, Disk *disk)
{
    Computer *computer = (Computer *) malloc(sizeof(Computer));
    if (!computer)
    {
        return NULL;
    }
    computer->scheduler = scheduler;
    computer->cpu = cpu;
    computer->disk = disk;
    return computer;
}

void freeComputer(Computer *computer)
{
    freeScheduler(computer->scheduler);
    freeCPU(computer->cpu);
    freeDisk(computer->disk);
    free(computer);
}

CPU *initCPU(int coreCount)
{
    CPU *cpu = malloc(sizeof(CPU));
    if (!cpu)
    {
        return NULL;
    }

    cpu->cores = malloc(coreCount * sizeof(Core *));
    if (!cpu->cores)
    {
        free(cpu);
        return NULL;
    }

    for (int i = 0; i < coreCount; i++)
    {
        cpu->cores[i] = malloc(sizeof(Core));
        if (!cpu->cores[i])
        {
            for (int j = 0; j < i; j++)
            {
                free(cpu->cores[j]);
            }
            free(cpu->cores);
            free(cpu);
            return NULL;
        }
        cpu->cores[i]->state = IDLE;
        cpu->cores[i]->previousState = IDLE; 
        cpu->cores[i]->timer = 0;
        cpu->cores[i]->previousTimer = 0;
        //No process at initialisation
        cpu->cores[i]->pid = -1; 
    }
    cpu->coreCount = coreCount;

    return cpu;
}

void freeCPU(CPU *cpu)
{
    for (int i = 0; i < cpu->coreCount; i++)
    {
        free(cpu->cores[i]);
    }
    free(cpu->cores);
    free(cpu);
}

Disk *initDisk(void)
{
    Disk *disk = malloc(sizeof(Disk));
    if (!disk)
    {
        return NULL;
    }

    disk->isIdle = true;
    // No process at initialisation
    disk->pid = -1;
    disk->isFree = true;

    return disk;
}

void freeDisk(Disk *disk)
{
    free(disk);
}

/* --------------------------functions implementation--------------------------- */

void interruptHandler(Computer *computer)
{
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }

    Disk *disk = computer->disk;
    CPU *cpu = computer->cpu;

    Core *interruptedCore = cpu->cores[FIRST_CORE];

    disk->isIdle = true;

    interruptedCore->previousState = interruptedCore->state;
    interruptedCore->state = INTERRUPT;

    interruptedCore->previousTimer = interruptedCore->timer;
    interruptedCore->timer = INTERRUPT_TIME;
    
}
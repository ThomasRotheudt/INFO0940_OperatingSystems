#ifndef computer_h
#define computer_h

#include <stdbool.h>

#include "process.h"
#include "schedulingAlgorithms.h"
#include "schedulingLogic.h"

#define SWITCH_OUT_DURATION 2 // Duration of the context switch out
#define SWITCH_IN_DURATION 1  // Duration of the context switch in

#define FIRST_CORE 0 // The core that will be interrupted

typedef struct CPU_t CPU;
typedef struct Core_t Core;
typedef struct Disk_t Disk;

/* ---------------------------- Computer struct ---------------------------- */

struct Computer_t
{
    Scheduler *scheduler;
    CPU *cpu;
    Disk *disk;
};


/* ------------------------------- CPU struct ------------------------------ */

typedef enum
{
    IDLE,
    WORKING,
    CONTEXT_SWITCHING_IN,
    CONTEXT_SWITCHING_OUT,
    INTERRUPT
} coreState;

struct CPU_t
{
    // list of cores
    Core **cores;
    int coreCount;
};

struct Core_t
{
    int pid;
    int timer;
    int previousTimer;
    coreState state;
    coreState previousState;
};


/* ------------------------------ Disk struct ------------------------------ */

struct Disk_t
{
    bool isFree;
    bool isIdle;
    int pid;
};

/* ------------------------- function definitions -------------------------
 * These functions respectively initialize and free the computer, CPU and Disk.
 * For the CPU, it initializes the number of cores (coreCount) that will be used.
 */

Computer *initComputer(Scheduler *scheduler, CPU *cpu, Disk *disk);
void freeComputer(Computer *computer);

CPU *initCPU(int coreCount);
void freeCPU(CPU *cpu);

Disk *initDisk(void);
void freeDisk(Disk *disk);

/**
 * Handle the trigger of an intterupt by the disk
 * 
 * @param computer: the compute (scheduler, cpu, disk)
 */
void interruptHandler(Computer *computer);



#endif // computer_h

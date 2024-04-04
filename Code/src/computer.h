#ifndef computer_h
#define computer_h

#include <stdbool.h>

#include "process.h"
#include "schedulingAlgorithms.h"
#include "schedulingLogic.h"

#define SWITCH_OUT_DURATION 2 // Duration of the context switch out
#define SWITCH_IN_DURATION 1  // Duration of the context switch in

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
    WORKING
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
    int contextSwitchTimer;
    coreState state;
};


/* ------------------------------ Disk struct ------------------------------ */

struct Disk_t
{
    bool isIdle;
};

/* ------------------------- function definitions -------------------------
 * These functions respectively initialize and free the computer, CPU and Disk.
 * For the CPU, it initializes the number of cores (coreCount) that will be used.
 */

Computer *initComputer(Scheduler *scheduler, CPU *cpu, Disk *disk);
void freeComputer(Computer *computer);

CPU *initCPU(int coreCount);
void freeCPU(CPU *cpu);

//!-------------------------------------------------------------------------------//
//TODO handle the interrupts in the simulation
/*The cpu check if a process on the cores have an IO interrupt if so 
put this process on the disk (if idle) remove it from the core and put it in the waiting queue. 

If the disk has finished the IO operation trigger a flag and remove the process from the wait queue
put the process at the beginning of the ready queue in which it was (thanks to the node of the waiting queue)
*/
void interruptHandler(Workload* Workload,CPU *cpu, Scheduler *scheduler, Disk *disk);
//!-------------------------------------------------------------------------------//

Disk *initDisk(void);
void freeDisk(Disk *disk);



#endif // computer_h

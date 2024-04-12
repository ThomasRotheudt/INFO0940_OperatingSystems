// In this file, you should modify the main loop inside launchSimulation and
// use the workload structure (either directly or through the getters and
// setters).

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "simulation.h"
#include "process.h"
#include "utils.h"
#include "computer.h"
#include "schedulingLogic.h"

#define MAX_CHAR_PER_LINE 500


/* --------------------------- struct definitions -------------------------- */

/**
 * The ProcessEvent strcut represent processes events as they are in the input
 * file (CPU or IO). They are represented as a linked list where each event
 * points to the next one.
 */
typedef struct ProcessEvent_t ProcessEvent;
/**
 * The ProcessSimulationInfo struct contains all the input file information
 * and the advancement time of a particular process. The Workload struct
 * contains an array of ProcessSimulationInfo.
 */
typedef struct ProcessSimulationInfo_t ProcessSimulationInfo;

typedef enum
{
    CPU_BURST,
    IO_BURST, // For simplicity, we'll consider that IO bursts are blocking (among themselves)
} ProcessEventType;

struct ProcessEvent_t
{
    ProcessEventType type;
    int time; // Time at which the event occurs. /!\ time relative to the process
    ProcessEvent *nextEvent; // Pointer to the next event in the queue
};

struct ProcessSimulationInfo_t
{
    PCB *pcb;
    int startTime;
    int processDuration; // CPU + IO !
    int advancementTime; // CPU + IO !
    ProcessEvent *nextEvent; // Pointer to the next event after the current one
};

struct Workload_t
{
    ProcessSimulationInfo **processesInfo;
    int nbProcesses;
};


/* ---------------------------- static functions --------------------------- */

/**
 * Return the index of the process with the given pid in the array of processes
 * inside the workload.
 *
 * @param workload: the workload
 * @param pid: the pid of the process
 *
 * @return the index of the process in the workload
 */
static int getProcessIndex(Workload *workload, int pid);

/**
 * Set the advancement time of the process with the given pid in the workload.
 *
 * @param workload: the workload
 * @param pid: the pid of the process
 * @param advancementTime: the new advancement time
 */
static void setProcessAdvancementTime(Workload *workload, int pid, int advancementTime);

/*
 * Returns true if all processes in the workload have finished
 * (advancementTime == processDuration).
 *
 * @param workload: the workload
 * @return true if all processes have finished, false otherwise
 */
static bool workloadOver(Workload *workload);

/**
 * Check all possible events of the simultation. If a new process must be add to the scheduler, if a process has an IO event at the current time on a core
 * or on the disk for the CPU event (trigger an interrupt in that case), and finally check the scheduling events. 
 * 
 * @param computer: all composant of the computer (scheduler, cpu, and disk)
 * @param workload: the workload
 * @param time: the current time unit of the simulation
 */
static void checkEvents(Computer *computer, Workload *workload, int time, AllStats *stats);

/**
 * Assign processes to ressources if possible thanks to the scheduling of the processes
 * 
 * @param computer: all composant of the computer (scheduler, cpu, and disk)
 */
static void assigningRessources(Computer *computer, Workload *workload, AllStats *stats);

static void updateValue(Computer *computer, Workload *workload, AllStats *stats);

static void addAllProcessesToStats(AllStats *stats, Workload *workload);

/**
 * Compare function used in qsort before the main simulation loop. If you don't
 * use qsort, you can remove this function.
 *
 * @param a, b: pointers to ProcessSimulationInfo to compare
 *
 * @return < 0 if process a is first, > 0 if b is first, = 0 if they start at
 *         the same time
 */
static int compareProcessStartTime(const void *a, const void *b);


static void fullFillGraph(ProcessGraph *graph, Computer *computer, Workload *workload, int time);

/* -------------------------- getters and setters -------------------------- */

int getProcessCount(const Workload *workload)
{
    return workload->nbProcesses;
}

int getPIDFromWorkload(Workload *workload, int index)
{
    return workload->processesInfo[index]->pcb->pid;
}

int getProcessStartTime(Workload *workload, int pid)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) == pid)
        {
            return workload->processesInfo[i]->startTime;
        }
    }
    return -1;
}

int getProcessDuration(Workload *workload, int pid)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) == pid)
        {
            return workload->processesInfo[i]->processDuration;
        }
    }
    return -1;
}

int getProcessAdvancementTime(Workload *workload, int pid)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) == pid)
        {
            return workload->processesInfo[i]->advancementTime;
        }
    }
    return -1;
}

int getProcessNextEventTime(Workload *workload, int pid)
{
    int processNextEventTime = getProcessDuration(workload, pid); // relative to the process
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) != pid)
        {
            continue;
        }
        if (workload->processesInfo[i]->nextEvent)
        {
            processNextEventTime = workload->processesInfo[i]->nextEvent->time;
        }
        break;
    }
    return processNextEventTime;
}

int getProcessCurEventTimeLeft(Workload *workload, int pid)
{
    return getProcessNextEventTime(workload, pid) 
           - getProcessAdvancementTime(workload, pid);
}

ProcessState getProcessState(Workload *workload, int pid)
{
    return workload->processesInfo[getProcessIndex(workload, pid)]->pcb->state;
}

void setProcessState(Workload *workload, int pid, ProcessState state)
{
    workload->processesInfo[getProcessIndex(workload, pid)]->pcb->state = state;
}

static int getProcessIndex(Workload *workload, int pid)
{
    int processIndex = 0;
    for (; processIndex < workload->nbProcesses; processIndex++)
    {
        if (getPIDFromWorkload(workload, processIndex) != pid)
        {
            continue;
        }
        break;
    }

    return processIndex;
}

static ProcessEventType getProcessNextEventType(Workload *workload, int pid)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) == pid)
        {
            if (workload->processesInfo[i]->nextEvent)
            {
                return workload->processesInfo[i]->nextEvent->type;
            }
        }
    }
    return -1;
}

static void setProcessAdvancementTime(Workload *workload, int pid, int advancementTime)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) == pid)
        {
            workload->processesInfo[i]->advancementTime = advancementTime;
            return;
        }
    }
}

static void setProcessNextEvent(Workload *workload, int pid)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        if (getPIDFromWorkload(workload, i) == pid)
        {
            ProcessSimulationInfo *processInfo = workload->processesInfo[i];
            ProcessEvent *current = workload->processesInfo[i]->nextEvent;
            if(current)
            {
                processInfo->nextEvent = current->nextEvent;
                free(current);
                break;
            }
        }
    }
}

/* -------------------------- init/free functions -------------------------- */

Workload *parseInputFile(const char *fileName)
{
    printVerbose("Parsing input file...\n");
    FILE *file = fopen(fileName, "r");
    if (!file)
    {
        fprintf(stderr, "Error: could not open file %s\n", fileName);
        return NULL;
    }

    Workload *workload = (Workload *) malloc(sizeof(Workload));
    if (!workload)
    {
        fprintf(stderr, "Error: could not allocate memory for workload\n");
        fclose(file);
        return NULL;
    }

    char line[MAX_CHAR_PER_LINE];
    int nbProcesses = 0;
    // 1 line == 1 process
    while (fgets(line, MAX_CHAR_PER_LINE, file))
    {
        if (line[strlen(line) - 1] != '\n')
        {
            fprintf(stderr, "Error: line too long in the input file.\n");
            freeWorkload(workload);
            fclose(file);
            return NULL;
        }
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }
        nbProcesses++;
    }
    
    workload->processesInfo = (ProcessSimulationInfo **) malloc(
            sizeof(ProcessSimulationInfo *) * nbProcesses);
    if (!workload->processesInfo)
    {
        fprintf(stderr, "Error: could not allocate memory for processes info\n");
        freeWorkload(workload);
        fclose(file);
        return NULL;
    }

    workload->nbProcesses = 0;

    rewind(file);
    while (fgets(line, MAX_CHAR_PER_LINE, file)) // Read file line by line
    {
        if (line[0] == '#' || line[0] == '\n')
        {
            continue;
        }

        ProcessSimulationInfo *processInfo = (ProcessSimulationInfo *) malloc(
                sizeof(ProcessSimulationInfo));
        if (!processInfo)
        {
            fprintf(stderr, "Error: could not allocate memory for process info\n");
            freeWorkload(workload);
            fclose(file);
            return NULL;
        }

        processInfo->pcb = (PCB *) malloc(sizeof(PCB));
        if (!processInfo->pcb)
        {
            fprintf(stderr, "Error: could not allocate memory for PCB\n");
            free(processInfo);
            freeWorkload(workload);
            fclose(file);
            return NULL;
        }

        processInfo->pcb->state = READY;

        char *token = strtok(line, ",");
        processInfo->pcb->pid = atoi(token);

        token = strtok(NULL, ",");
        processInfo->startTime = atoi(token);

        token = strtok(NULL, ",");
        processInfo->processDuration = atoi(token);

        token = strtok(NULL, ",");
        processInfo->pcb->priority = atoi(token);

        processInfo->advancementTime = 0;

        token = strtok(NULL, "(");

        ProcessEvent *event = NULL;
        while (strstr(token, ",") || strstr(token, "[")) // Read events
        {
            if (strstr(token, "[")) // first event
            {
                event = (ProcessEvent *) malloc(sizeof(ProcessEvent));
                processInfo->nextEvent = event;
            }
            else
            {
                event->nextEvent = (ProcessEvent *) malloc(sizeof(ProcessEvent));
                event = event->nextEvent;
            }
            if (!event)
            {
                fprintf(stderr, "Error: could not allocate memory for event\n");
                free(processInfo->pcb);
                free(processInfo);
                freeWorkload(workload);
                fclose(file);
                return NULL;
            }

            token = strtok(NULL, ",");
            event->time = atoi(token);

            token = strtok(NULL, ")");

            if (token != NULL)
            {
                if (strstr(token, "CPU"))
                {
                    event->type = CPU_BURST;
                }
                else if (strstr(token, "IO"))
                {
                    event->type = IO_BURST;
                }
                else
                {
                    fprintf(stderr, "Error: Unknown operation type\n");
                }
            }

            event->nextEvent = NULL;
            token = strtok(NULL, "(");
        } // End of events
        workload->processesInfo[workload->nbProcesses] = processInfo;
        workload->nbProcesses++;
    } // End of file

    fclose(file);

    printVerbose("Input file parsed successfully\n");

    return workload;
}

void freeWorkload(Workload *workload)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        ProcessEvent *event = workload->processesInfo[i]->nextEvent;
        while (event)
        {
            ProcessEvent *nextEvent = event->nextEvent;
            free(event);
            event = nextEvent;
        }
        free(workload->processesInfo[i]->pcb);
        free(workload->processesInfo[i]);
    }
    free(workload->processesInfo);
    free(workload);
}


/* ---------------------------- other functions ---------------------------- */

void launchSimulation(Workload *workload, SchedulingAlgorithm **algorithms, int algorithmCount, int cpuCoreCount, ProcessGraph *graph, AllStats *stats)
{
    for (int i = 0; i < getProcessCount(workload); i++)
    {
        addProcessToGraph(graph, getPIDFromWorkload(workload, i));
    }
    setNbProcessesInStats(stats, getProcessCount(workload));

    Scheduler *scheduler = initScheduler(algorithms, algorithmCount);
    if (!scheduler)
    {
        fprintf(stderr, "Error: could not initialize scheduler\n");
        return;
    }

    CPU *cpu = initCPU(cpuCoreCount);
    if (!cpu)
    {
        fprintf(stderr, "Error: could not initialize CPU\n");
        freeScheduler(scheduler);
        return;
    }

    Disk *disk = initDisk();
    if (!disk)
    {
        fprintf(stderr, "Error: could not initialize disk\n");
        freeCPU(cpu);
        freeScheduler(scheduler);
        return;
    }

    Computer *computer = initComputer(scheduler, cpu, disk);
    if (!computer)
    {
        fprintf(stderr, "Error: could not initialize computer\n");
        freeDisk(disk);
        freeCPU(cpu);
        freeScheduler(scheduler);
        return;
    }

    addAllProcessesToStats(stats, workload);

    // You may want to sort processes by start time to facilitate the
    // simulation. Remove this line and the compareProcessStartTime if you
    // don't want to.
    qsort(workload->processesInfo, workload->nbProcesses, sizeof(ProcessSimulationInfo *), compareProcessStartTime);

    int time = 0;

    /* Main loop of the simulation.*/
    while (!workloadOver(workload)) // You probably want to change this condition
    {      
        checkEvents(computer, workload, time, stats);
        assigningRessources(computer, workload, stats);
        updateValue(computer, workload, stats);

        fullFillGraph(graph, computer, workload, time);

        time++;
    }

    freeComputer(computer);
}


/* ---------------------------- static functions --------------------------- */

static void checkEvents(Computer *computer, Workload *workload, int time, AllStats *stats)
{
    if (!computer)
    {
        fprintf(stderr, "The computer does not exist.\n");
        return;
    }

    Scheduler *scheduler = computer->scheduler;
    CPU *cpu = computer->cpu;
    Disk *disk = computer->disk;

    // Check if new processes can be add to the scheduler
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        int pid = getPIDFromWorkload(workload, i);
        ProcessStats *processStat = getProcessStats(stats, pid);
        ProcessSimulationInfo *process = workload->processesInfo[i];
        int startTime = getProcessStartTime(workload, pid);

        if (time == startTime)
        {
            PCB *pcb = process->pcb;
            // Add the process to the scheduler
            addProcessToScheduler(scheduler, pcb);
            // Set the arrival time stat
            processStat->arrivalTime = time;
            // Skip the event (0, CPU) as the process is in the ready queue
            setProcessNextEvent(workload, pcb->pid);
            // Trigger timer
            processStat->turnaroundTime = 0;
        }
    }

    // Check if a process on a core is terminated
    for (int i = 0; i < cpu->coreCount; i++)
    {
        Core *core = cpu->cores[i];

        if (core->state == WORKING)
        {
            int pid = core->pid;
            ProcessStats *processStat = getProcessStats(stats, pid);
            int processDuration = getProcessDuration(workload, pid);
            int advancementTime = getProcessAdvancementTime(workload, pid);

            if(processDuration <= advancementTime)
            {
                //Remove process from scheduler (in running queue)
                removeProcessFromScheduler(computer, pid, i);
                // Set the new state of the process
                setProcessState(workload, pid, TERMINATED);

                // Compute mean response time
                processStat->meanResponseTime /= (double) processStat->finishTime;
                // Set the finishTime state
                processStat->finishTime = time;
                // Compute turnaround
                processStat->turnaroundTime = processStat->finishTime - processStat->arrivalTime;
            }
        }
    }

    // Check scheduling events
    schedulingEvents(computer, workload, stats);

    // Check for the end of context switch or interrupt
    for (int i = 0; i < cpu->coreCount; i++)
    {
        Core *core = cpu->cores[i];
        if (core->timer == 0)
        {
            switch (core->state)
            {
                case CONTEXT_SWITCHING_IN:
                    core->state = WORKING;
                    setProcessState(workload, core->pid, RUNNING);
                    break;

                case CONTEXT_SWITCHING_OUT:
                    core->state = IDLE;
                    break;
                    
                case INTERRUPT:
                    setProcessState(workload, disk->pid, READY);
                    returnFromWaitQueue(scheduler, disk->pid);
                    disk->isFree = true;
                    disk->pid = -1;

                    cpu->cores[FIRST_CORE]->state = cpu->cores[FIRST_CORE]->previousState;
                    cpu->cores[FIRST_CORE]->timer = cpu->cores[FIRST_CORE]->previousTimer;
                    if (cpu->cores[FIRST_CORE]->state == WORKING)
                    {
                        setProcessState(workload, cpu->cores[FIRST_CORE]->pid, RUNNING);
                    }
                    break;

                default:
                    break;
            }
        }
    }

    // Check for preemption
    preemption(computer, workload, stats);

    // Check process events that runs on cpu
    for (int i = 0; i < cpu->coreCount; i++)
    {
        Core *core = cpu->cores[i];
        ProcessStats *processStat = getProcessStats(stats, core->pid);

        if (core->state == WORKING)
        {
            int pid = core->pid;
            int advancementTime = getProcessAdvancementTime(workload, pid);
            int nextEventTime = getProcessNextEventTime(workload, pid);

            if ((advancementTime == nextEventTime) && getProcessNextEventType(workload, pid) == IO_BURST)
            {
                // Add the process to the waiting queue
                addProcessToWaitingQueue(scheduler, pid);
                // Update the next event of the process in the workload
                setProcessNextEvent(workload, pid);
                // Update the state of the proces to WAITING
                setProcessState(workload, pid, WAITING);

                // Set the context switch out of the core
                core->timer = SWITCH_OUT_DURATION;
                // Set the state of the core to context switching out
                core->state = CONTEXT_SWITCHING_OUT;
                //Remove the pid from the core
                core->pid = -1;
                // Increment the context switch stat
                processStat->nbContextSwitches++;


            }
        }
    }

    // Check process events that runs on disk
    if (!disk->isIdle)
    {
        int pid = disk->pid;
        int advancementTime = getProcessAdvancementTime(workload, pid);
        int nextEventTime = getProcessNextEventTime(workload, pid);



        if ((advancementTime == nextEventTime) && getProcessNextEventType(workload, pid) == CPU_BURST)
        {
            Core *interruptedCore = cpu->cores[FIRST_CORE];
            if (interruptedCore->state == WORKING)
            {
                setProcessState(workload, interruptedCore->pid, READY);
            }
            interruptHandler(computer);
            // Update the next event of the process in the workload
            setProcessNextEvent(workload, pid);
        }
    }
}

static void assigningRessources(Computer *computer, Workload *workload, AllStats *stats)
{
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }

    CPU *cpu = computer->cpu;
    Disk *disk = computer->disk;

    for (int i = 0; i < cpu->coreCount; i++)
    {
        Core *core = cpu->cores[i];

        if (core->state == IDLE)
        {
            setProcessToCore(computer, workload, i, stats);
        }
    }
    
    if (disk->isFree)
    {
        setProcessToDisk(computer);
    }
}

static void updateValue(Computer *computer, Workload *workload, AllStats *stats)
{
    if (!computer)
    {
        fprintf(stderr, "Error: The computer does not exist.\n");
        return;
    }

    Scheduler *scheduler = computer->scheduler;
    CPU *cpu = computer->cpu;
    Disk *disk = computer->disk;

    // Update value on CPU
    for (int i = 0; i < cpu->coreCount; i++)
    {
        Core *core = cpu->cores[i];
        ProcessStats *processStat = getProcessStats(stats, core->pid);
        if (core->timer > 0)
        {
            core->timer--;
        }
        else if (core->state == WORKING)
        {
            processStat->cpuTime++;
            int previousAdvancement = getProcessAdvancementTime(workload, core->pid);
            setProcessAdvancementTime(workload, core->pid, previousAdvancement + 1);
        }
    }

    //Update value in scheduler
    updateSchedulingValue(scheduler, workload, stats);

    // Update value on disk
    if (!disk->isIdle)
    {
        int previousAdvancement = getProcessAdvancementTime(workload, disk->pid);
        setProcessAdvancementTime(workload, disk->pid, previousAdvancement + 1);
    }
}

static bool workloadOver(Workload *workload)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        int pid = getPIDFromWorkload(workload, i);
        if (getProcessState(workload, pid) != TERMINATED)
        {
            return 0;
        }
    }

    return 1;
}

static void addAllProcessesToStats(AllStats *stats, Workload *workload)
{
    for (int i = 0; i < workload->nbProcesses; i++)
    {
        ProcessStats *processStats = (ProcessStats *) malloc(sizeof(ProcessStats));
        if (!processStats)
        {
            fprintf(stderr, "Error: could not allocate memory for process stats\n");
            return;
        }
        processStats->processId = getPIDFromWorkload(workload, i);
        processStats->priority = workload->processesInfo[i]->pcb->priority;
        processStats->arrivalTime = 0;
        processStats->finishTime = 0;
        processStats->turnaroundTime = 0;
        processStats->cpuTime = 0;
        processStats->waitingTime = 0;
        processStats->meanResponseTime = 0;
        // You could want to put this field to -1
        processStats->nbContextSwitches = 0;

        addProcessStats(stats, processStats);
    }
}

static int compareProcessStartTime(const void *a, const void *b)
{
    const ProcessSimulationInfo *infoA = *(const ProcessSimulationInfo **)a;
    const ProcessSimulationInfo *infoB = *(const ProcessSimulationInfo **)b;

    if (infoA->startTime < infoB->startTime)
    {
        return -1;
    }
    else if (infoA->startTime > infoB->startTime)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static void fullFillGraph(ProcessGraph *graph, Computer *computer, Workload *workload, int time)
{
    CPU *cpu = computer->cpu;

    for (int i = 0; i < workload->nbProcesses; i++)
        {
            int pid = getPIDFromWorkload(workload, i);
            if (time >= getProcessStartTime(workload, pid))
            {
                ProcessState state = getProcessState(workload, pid);
                if (state == RUNNING)
                {
                    for (int i = 0; i < cpu->coreCount; i++)
                    {
                        Core *core = cpu->cores[i];
                        if (pid == core->pid)
                        {
                            addProcessEventToGraph(graph, pid, time, state, i);
                        }
                    }
                }
                else
                {
                    addProcessEventToGraph(graph, pid, time, state, 0);
                }
            }
        }
}
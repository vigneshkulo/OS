#include <stdio.h>
#include <stdlib.h>
#include "simos.h"


//============================================
// context switch, switch in or out a process pid

void context_in (pid)
{ CPU.Pid = pid;
  CPU.PC = PCB[pid]->PC;
  CPU.AC = PCB[pid]->AC;
  CPU.Mbase = PCB[pid]->Mbase;
  CPU.MDbase = PCB[pid]->MDbase;
  CPU.Mbound = PCB[pid]->Mbound;
  CPU.spoolPtr = PCB[pid]->spoolPtr;
  CPU.spoolPos = PCB[pid]->spoolPos;
  CPU.exeStatus = PCB[pid]->exeStatus;
}

void context_out (pid)
{ PCB[pid]->PC = CPU.PC;
  PCB[pid]->AC = CPU.AC;
  PCB[pid]->spoolPos = CPU.spoolPos;
  PCB[pid]->exeStatus = CPU.exeStatus;
}


//=========================================================================
// ready queue management
// implement as an array with head and tail pointers
// the number of processes that can be handled in the system is limited
// so having a maximum number of ready processes is fine
//=========================================================================

#define nullReady 0
#define maxReady 100
       // The ready queue capacity, gives a limit on number of
       // ready processes the system can handle

int readyList[maxReady];
int readyCount = 0;
int readyHead = 0;
int readyTail = 0;
                   // head got executed next, insert a new process to tail

void insert_ready (pid)
int pid;
{ 
  if (readyCount >= maxReady)
    { printf ("Ready queue is full, process rejected!!!\n"); }
  else { readyList[readyTail] = pid;
         readyCount++;
         readyTail = (readyTail+1) % maxReady;
       }
}

int get_ready ()
{
  if (readyCount == 0)
    { printf ("No ready process now!!!\n"); return (nullReady); }
  else return (readyList[readyHead]);
}

void remove_ready ()
{
  if (readyCount == 0)
    { printf ("Incorrect removal of ready list!!!\n"); exit(-1); }
  readyCount--;
  readyHead = (readyHead+1) % maxReady;
}

void dump_ready_queue ()
{ int i, next;

  printf ("******************** Ready Queue Dump\n");
  printf ("readyCount = %d; readyHead = %d; readyTail = %d\n",
           readyCount, readyHead, readyTail);
  for (i=0; i<readyCount; i++)
  { next = (readyHead+i) % maxReady;
    printf ("readyList[%d] = %d\n", next, readyList[next]);
  }
}


//=========================================================================
// doneWait list management
// processes that has finished waiting can be inserted into doneWait list
//   -- when adding process to doneWait list, should set doneWaitInterrupt
//      interrupt handler moves processes in doneWait list to ready queue
// since there is no list search, we just use a link list structure
//=========================================================================

typedef struct doneWaitNode
{ int pid;
  struct doneWaitNode *next;
} doneWaitNode;

doneWaitNode *doneWaitHead = NULL;
doneWaitNode *doneWaitTail = NULL;

void insert_doneWait_process (pid)
int pid;
{ doneWaitNode *node;

  node = (doneWaitNode *) malloc (sizeof (doneWaitNode));
  node->pid = pid;
  node->next = NULL;
  if (doneWaitTail == NULL) // doneWaitHead would be NULL also
    { doneWaitTail = node; doneWaitHead = node; }
  else // insert to tail
    { doneWaitTail->next = node; doneWaitTail = node; }
}

// move all processes in doneWait list to ready queue, empty the list
// before moving, perform necessary doneWait actions

void doneWait_moveto_ready ()
{ doneWaitNode *temp;

  while (doneWaitHead != NULL)
  { temp = doneWaitHead;
    insert_ready (temp->pid);
    doneWaitHead = temp->next;
    free (temp);
  }
  doneWaitTail = NULL;
}


void dump_doneWait_list ()
{ doneWaitNode *node;

  node = doneWaitHead;
  printf ("doneWait List = ");
  while (node != NULL) { printf ("%d, ", node->pid); node = node->next; }
  printf ("\n");
}

//=========================================================================
// Some support functions for PCB 
// PCB related definitions are in simos.h
//=========================================================================

int get_PCB ()
{ int pid;

  pid = currentPid; currentPid++;
  PCB[pid] = (typePCB *) malloc ( sizeof(typePCB) );
  PCB[pid]->Pid = pid;
  return (pid);
}

void free_PCB (pid)
int pid;
{
  free (PCB[pid]); 
  if (Debug) printf ("Free PCB effect: %p\n", PCB[pid]);
  PCB[pid] = NULL;
}

void dump_PCB (pid)
int pid;
{
	printf ("* --------------------------------------\n");
	printf ("* PCB Dump for Process: %d\n", pid);
	printf ("* --------------------------------------\n");
	printf ("  Pid 		: %d\n", PCB[pid]->Pid);
	printf ("  PC 		: %d\n", PCB[pid]->PC);
	printf ("  AC 		: %.2f\n", PCB[pid]->AC);
	printf ("  Mbase 	: %p\n", PCB[pid]->Mbase);
	printf ("  MDbase 	: %x\n", PCB[pid]->MDbase);
	printf ("  Mbound 	: %x\n", PCB[pid]->Mbound);
	printf ("  spoolPtr 	: %p\n", PCB[pid]->spoolPtr);
	printf ("  spoolPos 	: %d\n", PCB[pid]->spoolPos);
	printf ("  exeStatus 	: %d\n", PCB[pid]->exeStatus);
	printf ("  numInstr 	: %d\n", PCB[pid]->numInstr);
	printf ("  numData 	: %d\n", PCB[pid]->numData);
	printf ("  numStaticData	: %d\n", PCB[pid]->numStaticData);
	printf("* --------------------------------------\n");
}


//=========================================================================
// process management
//=========================================================================

#define OPifgo 5
#define idleMsize 3
#define idleNinstr 2

// this function initializes the idle process
// idle process has only 1 instruction, ifgo (2 words) and 1 data
// the ifgo condition is always true and will always go back to 0

void init_idle_process ()
{ 
  // create and initialize PCB for the idle process
  PCB[idlePid] = (typePCB *) malloc ( sizeof(typePCB) );
  allocate_memory (idlePid, idleMsize, idleNinstr);

  PCB[idlePid]->Pid = idlePid;
  PCB[idlePid]->PC = 0;
  PCB[idlePid]->AC = 0;
  PCB[idlePid]->numInstr = 2;
  PCB[idlePid]->numStaticData = 1;
  PCB[idlePid]->numData = 1;
  if (Debug) dump_PCB (idlePid);

  // load instructions and data into memory
  load_instruction (idlePid, 0, OPifgo, 0);
  load_instruction (idlePid, 1, OPifgo, 0);
  load_data (idlePid, 0, 1.0);
  if (Debug) dump_memory (idlePid);
}

void initialize_process ()
{
        printf("* --------------------------------------\n");
        printf("* Initializing Process\n");
        printf("* --------------------------------------\n");
	currentPid = 2;  // the next pid value to be used

	/* Allocating Memory for OS*/
	PCB[0] = (typePCB *) malloc ( sizeof(typePCB) );
	allocate_memory (0, OSmemSize, 0);
	init_idle_process ();
        printf("* --------------------------------------\n");
}

void submit_process (fname)
char *fname;
{
  FILE *fprog;
  int pid, msize, numinstr, numdata;
  int ret, i, opcode, operand;
  float data;

  // a program in file fname is submitted,
  // it needs msize memory, has numinstr of instructions and numdata of data
  // assign pid, allocate PCB, memory, spool
  fprog = fopen (fname, "r");
  if (fprog == NULL)
    { printf ("Incorrect program name: %s! Submission failed!!!\n", fname);
      return;
    }
  fscanf (fprog, "%d %d %d\n", &msize, &numinstr, &numdata);

	pid = get_PCB ();
	ret = allocate_memory (pid, msize, numinstr);
	if(-1 == ret || -2 == ret)
	{
		printf ("* Invalid Memory Size (or) Pid Limit Reached\n", fname);
		printf("* --------------------------------------\n");
		free_PCB(pid);
		fclose(fprog);
		return;
	}
	allocate_spool (pid);

  // initialize PCB 
  //   memory related info has been initialized in memory.c
  //   Pid has been initialized in get_PCB
  PCB[pid]->PC = 0;
  PCB[pid]->AC = 0;
  PCB[pid]->exeStatus = eReady;
  PCB[pid]->numInstr = numinstr;
  PCB[pid]->numStaticData = numdata;
  PCB[pid]->numData = numdata;
  if (Debug) dump_PCB (pid);

  // load instructions and data of the process to memory 
  for (i=0; i<numinstr; i++)
  { fscanf (fprog, "%d %d\n", &opcode, &operand);
    if (Debug) printf ("* Process %d load instruction: %d, %d, %d\n",
                                   pid, i, opcode, operand);
    ret = load_instruction (pid, i, opcode, operand);
    if (ret == mError) PCB[pid]->exeStatus = eError;
  }
  for (i=0; i<numdata; i++)
  { fscanf (fprog, "%f\n", &data);
    ret = load_data (pid, i, data);
    if (Debug) printf ("* Process %d load data: %d, %.2f\n", pid, i, data);
    if (ret == mError) PCB[pid]->exeStatus = eError;
  }

	// put process into ready list
	if (PCB[pid]->exeStatus == eReady) 
	{
		insert_ready (pid);
		load_swap_to_mem(pid);
	}
	else printf ("Process %d loading was unsuccessful!!!\n", pid);
	close (fprog);
}

void clean_process (pid)
int pid;
{
  free_memory (pid);
  free_swap_memory (pid);
  free_spool (pid);
  free_PCB (pid);  // PCB has to be freed last, other frees use PCB info
} 

void end_process (pid)
int pid;
{
  PCB[pid]->exeStatus = CPU.exeStatus;
  PCB[pid]->spoolPos = CPU.spoolPos;
    // PCB[pid] is not updated, no point to do a full context switch
    // but the information to be used by the spooler needs to be updated 

  if (CPU.exeStatus == eError)
    { printf ("Process %d has an error, dumping its states\n", pid);
      dump_PCB (pid); dump_memory (pid); dump_spool (pid);
    }
  print_spool (pid); 
  clean_process (pid); 
    // if there is a real printer, printing would be slow
    // cpu should continue with executing other processes
    // and cleaning is only done after the printer acknowledges
}

void execute_process ()
{ int pid;
  genericPtr event;

  pid = get_ready ();
  if (pid != nullReady)
  { context_in (pid);
    CPU.exeStatus = eRun;
    event = add_timer (cpuQuantum, CPU.Pid, actTQinterrupt, oneTimeTimer);
    cpu_execution (); 
    if (CPU.exeStatus == eReady)
      { context_out (pid);
        remove_ready (); insert_ready (pid);
      }
    else if (CPU.exeStatus == ePFault) 
      { context_out (pid);
        remove_ready (); deactivate_timer (event);
        init_pagefault_handler (CPU.Pid);
      }
    else if (CPU.exeStatus == eWait) 
      { context_out (pid);
        remove_ready (); deactivate_timer (event); 
      }
    else // CPU.exeStatus == eError or eEnd
      { remove_ready (); end_process (pid); deactivate_timer (event); }

    // ePFault and eWait has to be handled differently
    // in ePFfault, memory has to handle the event 
    // in eWait, CPU directly execute IO libraries and initiate IO
    //
    // In case of eWait/ePFault/eError/eEnd, timer should be deactivated,
    // otherwise, it has the potential of impacting the next round of exe
    // but if time quantum just expires when the above cases happends,
    // event would have just been freed, our deactivation can be dangerous
  }
  else // no ready process in the system
  { context_in (idlePid);
    CPU.exeStatus = eRun;
    add_timer (idleQuantum, CPU.Pid, actTQinterrupt, oneTimeTimer);
    cpu_execution (); 
  }
}



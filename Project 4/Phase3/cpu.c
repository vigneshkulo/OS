#include <stdio.h>
#include <stdlib.h>
#include "simos.h"


#define OPload 2
#define OPadd 3
#define OPsum 4
#define OPifgo 5
#define OPstore 6
#define OPprint 7
#define OPsleep 8
#define OPend 1


void initialize_cpu ()
{ // Generally, cpu goes to a fix location to fetch and execute OS
  CPU.interruptV = 0;
  CPU.numCycles = 0;
}

void dump_registers ()
{ 
        printf ("* --------------------------------------\n");
        printf ("* Register Dump\n");
        printf ("* --------------------------------------\n");
	printf (" Pid		: %d\n", CPU.Pid);
	printf (" PC		: %d\n", CPU.PC);
	printf (" AC		: %.2f\n", CPU.AC);
	printf (" MBR		: %.2f\n", CPU.MBR);
	printf (" IRopcode	: %d\n", CPU.IRopcode);
	printf (" IRoperand 	: %d\n", CPU.IRoperand);
	printf (" Mbase		: %p\n", CPU.Mbase);
	printf (" MDbase 	: %d\n", CPU.MDbase);
	printf (" Mbound 	: %d\n", CPU.Mbound);
	printf (" spoolPos 	: %d\n", CPU.spoolPos);
	printf (" exeStatus 	: %d\n", CPU.exeStatus);
	printf (" InterruptV 	: %x\n", CPU.interruptV);
	printf (" numCycles 	: %d\n", CPU.numCycles);
        printf ("* --------------------------------------\n");
}

void set_interrupt (bit)
unsigned bit;
{
  // CPU.interruptV = CPU.interruptV | bit;
  __sync_or_and_fetch (&CPU.interruptV, bit);
}

void handle_interrupt ()
{
  if ((CPU.interruptV & ageInterrupt) == ageInterrupt)
    memory_agescan ();
  if ((CPU.interruptV & doneWaitInterrupt) == doneWaitInterrupt)
    doneWait_moveto_ready ();  
     // interrupt may overwrite, move all IO done processes (maybe more than 1)
  if ((CPU.interruptV & tqInterrupt) == tqInterrupt)
    if (CPU.exeStatus == eRun) CPU.exeStatus = eReady;
      // need to do this last, in case exeStatus is changed for other reasons

  // all interrupt bits have been taken care of, reset vector
  CPU.interruptV = 0;
  if (Debug) 
    printf ("* Interrup Handler: Pid = %d; Interrupt = %x; ExeStatus = %d\n",
            CPU.Pid, CPU.interruptV, CPU.exeStatus); 
}


void increase_numcycles ()
{ 
  CPU.numCycles++;

  if (CPU.numCycles > maxCPUcycles)
    { printf ("CPU cycle count exceeds its limit!!!\n"); exit(-1); }
  // in a real system, timer is checked on clock cycles
  // here we use CPU cycle for timer, so timer check is done here
  check_timer (); 
}

// CPU executes the designated process for num of instructions
// num is to simulate process execution time out

void cpu_execution ()
{ float sum, temp;
  int mret, spret, gotoaddr;
  char str[32]; 

  // perform all memory fetches, analyze memory conditions all here
  while (CPU.exeStatus == eRun)
  { 
	printf("  -----------------------------------------------------------------------------------------------------\n");
	mret = get_instruction (CPU.PC);
    if (mret == mError) CPU.exeStatus = eError;
    else if (mret == mPFault) CPU.exeStatus = ePFault;
    else if (CPU.IRopcode != OPend && CPU.IRopcode != OPsleep)
            // fetch data, but exclude OPend and OPsleep, which has no data
    { 
	mret = get_data (CPU.IRoperand); 
      if (mret == mError) CPU.exeStatus = eError;
      else if (mret == mPFault) CPU.exeStatus = ePFault;
      else if (CPU.IRopcode == OPifgo)
      { mret = get_instruction (CPU.PC+1);
        if (mret == mError) CPU.exeStatus = eError;
        else if (mret == mPFault) CPU.exeStatus = ePFault;
        else { CPU.PC++; CPU.IRopcode = OPifgo; }
      }
    }

if (Debug)
{ 
	printf ("* [%d]: Process %d executing: ", CPU.numCycles, CPU.Pid);
	printf ("PC: %d, OPcode: %d, Operand: %d, AC: %.2f, MBR: %.2f\n",
              CPU.PC, CPU.IRopcode, CPU.IRoperand, CPU.AC, CPU.MBR);
}

    // if it is eError or eEnd, then does not matter 
    // if it is page fault, then AC, PC should not be changed
    // because the instruction should be re-executed
    if (CPU.exeStatus == eRun)
    { switch (CPU.IRopcode)
      { case OPload:
          CPU.AC = CPU.MBR; break;
        case OPadd:
          CPU.AC = CPU.AC + CPU.MBR; break;
        case OPsum:
          temp = CPU.MBR;  sum = 0;
          while (temp > 0) { sum = sum+temp; temp = temp-1; }
          CPU.AC = CPU.AC + sum;
          break;
        case OPifgo:  // conditional goto, need two instruction words
          gotoaddr = CPU.IRoperand; // this is the second operand
          if (Debug) printf ("* Process %d executing: Goto %d, If %d,%f\n",
                              CPU.Pid,  gotoaddr, CPU.IRoperand, CPU.MBR);
          if (CPU.MBR > 0) CPU.PC = gotoaddr - 1;
                               // Note: PC will be ++, so set to 1 less 
          break;
        case OPstore:
          put_data (CPU.IRoperand); break;
        case OPprint:
          sprintf (str, "M[%d]=%.2f\n", CPU.IRoperand, CPU.MBR);
              // if the printing becomes longer, change str size 
          spret = spool (str);
          if (spret == spError) CPU.exeStatus = eError;
          break;
        case OPsleep:
          add_timer (CPU.IRoperand, CPU.Pid, actReadyInterrupt, oneTimeTimer);
          CPU.exeStatus = eWait; break;
        case OPend:
          CPU.exeStatus = eEnd; break;
        default:
          printf ("Illegitimate OPcode in process %d\n", CPU.Pid);
          CPU.exeStatus = eError;
      }
      CPU.PC++;
    }

    // no matter whether there is a page fault or an error,
    // should handle cycle increment and interrupt
    increase_numcycles ();
    if (CPU.interruptV != 0) handle_interrupt ();
  }
}


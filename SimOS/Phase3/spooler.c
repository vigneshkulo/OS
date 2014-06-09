#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simos.h"


// Note that the spoolPos in PCB[pid] may not be up to date
// It will only be updated after each context switch out
void dump_spool (pid)
int pid;
{ int last;
  int i;

  last = PCB[pid]->spoolPos;
  printf ("********** Spool Dump for Process %d with size %d\n", pid, last);
  for (i=0; i<last; i++) 
    if (PCB[pid]->spoolPtr[i] < 32 || PCB[pid]->spoolPtr[i] > 126)
      // printable characters are between 32 and 126
      printf ("\\%d\\", PCB[pid]->spoolPtr[i]);
    else printf ("%c", PCB[pid]->spoolPtr[i]);
  printf ("\n");
}

int spool (str)
char *str;
{ int len; 

  len = strlen (str);
  if (Debug) printf ("  Spool string: %s; len = %d\n", str, len);
  if (CPU.spoolPtr == NULL)
    { printf ("Spool for Process %d does not exist!!!\n", CPU.Pid);
      return (spError);
    }
  else if (CPU.spoolPos+len > spoolPsize) 
    { printf ("Spool space overflow: Process %d Length %d!!!\n", CPU.Pid, len);
      return (spError);
    }
  else { strncpy (CPU.spoolPtr+CPU.spoolPos, str, len);
         CPU.spoolPos = CPU.spoolPos + len;
         return (spNormal);
       }
}

void allocate_spool (pid)
int pid;
{ 
  PCB[pid]->spoolPtr = malloc (spoolPsize);
  PCB[pid]->spoolPos = 0;
}

void free_spool (pid)
int pid;
{
  free (PCB[pid]->spoolPtr);
  if (Debug) printf ("Free spoolPtr effect: %p\n", PCB[pid]->spoolPtr);
  PCB[pid]->spoolPtr = NULL;
}

// printer call should be a send, send the print requst to the printer
// should select a free printer to send the print job to
// if one printer has an error return, send to another printer
//
void print_spool (pid)
int pid;
{ int ret;

  ret = printer (pid, PCB[pid]->exeStatus,
                 PCB[pid]->spoolPtr, PCB[pid]->spoolPos);
}


//=====================================================
// printer function
// should recieve request from spooler, process it, ack it

int printer (pid, status, prstr, len)
int pid, status;
char *prstr;
int len;
{ int i;

  printf ("\n======== Printout for Process %d ========\n", pid);
  if (status == eError)
    printf ("Process %d had encountered error in execution!!!\n", pid);
  else  // was eEnd
    printf ("Process %d had completed successfully!!!\n", pid);
  printf ("=========================================\n\n");

  for (i=0; i<len; i++) printf ("%c", prstr[i]);
  printf ("=========================================\n\n");

  return (prNormal);
    // should simulate failure situations: out of paper, toner low, etc.
}



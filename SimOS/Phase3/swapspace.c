#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simos.h"

#define fixMsize 256  // each process has a fixed memory size
                      // first part of the process memory is for program
                      // remaining is for data
                      // this size is actually 256*4 = 1024B
#define dataSize 4    // each memroy unit is of 4 bytes
#define OSsize 256 // first physical memory segment is for OS
                   // so first user physical memory starts at 1024
#define DATA	0
#define INST	1

#define WORDSIZE	32

int numSwapPP = 0;
int swapAllocated = 0;

#define opcodeShift 24
#define operandMask 0x00ffffff

//==========================================
// memory management functions -- general
mType* get_page_start_addr(int pid, int pgN)
{
	int* swapPageTableLocal = NULL;
	swapPageTableLocal = pageTablePtr[pid];
	if(pid == freeListSwap[swapPageTableLocal[pgN]])
	{
		return (mType*)&swapPageFrame[(swapPageTableLocal[pgN] * pageSize)];
	}
	printf("* Requested Page %d owned by %d\n", pgN, freeListSwap[swapPageTableLocal[pgN]]);
	return NULL;
}

void initialize_swap_memory ()
{ 
	int i;
        numSwapPages = swapSize/pageSize;
        printf("* --------------------------------------\n");
        printf("* Initializing Swap Memory\n");
        printf("* --------------------------------------\n");
        printf("* No. of Pages (%d / %d): %d\n\n", swapSize, pageSize, numSwapPages);

        freeListSwap = (int*) malloc(numSwapPages * sizeof(int));

//	printf("* Free List Swap : ");
        for(i = 0; i < numSwapPages; i++)
        {
                freeListSwap[i] = -1;
//		printf("%2d ", i);
        }
//	printf("\n");
        swapPageFrame = (int*)malloc(swapSize * WORDSIZE);
}

int allocate_swap_memory (pid, msize, numinstr)
int pid, msize, numinstr;
{
	int *pageTab;
	int i, j, pageNum;
        printf("* --------------------------------------\n");
        printf("* Allocate Swap Memory: %d words by Process: %d\n", msize, pid);
        printf("* --------------------------------------\n");
	if (pid >= maxProcess) 
	{ 
		printf ("Invalid pid: %d\n", pid); 
		return(mError); 
	}
	else if (msize > fixMsize) 
	{ 
		printf ("Invalid memory size %d for process %d\n", msize, pid);
		return(mError);                                                
	}
	else 
	{ 
		numSwapPP = msize/pageSize;
		if(0 != msize%pageSize) numSwapPP++;

		if(numSwapPP > (numSwapPages - swapAllocated))
		{
			printf("* Pages unavailable\n");
			printf("* --------------------------------------\n");
			return(mSwapPageUnavail);                                                
		}
		printf("* #Pages Requested : %d\n", numSwapPP);
		pageTab = (int*) malloc(numSwapPP * sizeof(int));
		printf("* #Frames Allocated:");
		for(i = 0; i < numSwapPP; i++)
		{
			for(j = 0; j < numSwapPages; j++)
			{
				if(-1 == freeListSwap[j])
				{
					printf(" %d ", j);
					freeListSwap[j] = pid;
					pageTab[i] = j;
					swapAllocated++;
					break;
				}
			}
		}
		printf("\n");
		pageTablePtr[pid] = pageTab;
		
		#if 0
		printf("* --------------------------------------\n");
		printf("* Free Swap List  : ");
		for(i = 0; i < numSwapPages; i++)
		{
			if(-1 == freeListSwap[i]) printf("%2d ", i);
		}
		printf("\n* --------------------------------------\n");
		#endif
		return (mNormal);
	}
}

int free_swap_memory (pid)
int pid;
{
	int i, j;
	int *pageTab;
	pageTab = pageTablePtr[pid];
	if(NULL != pageTab) free(pageTab);
	pageTablePtr[pid] = NULL;

	for(j = 0; j < numSwapPages; j++)
	{
		if(pid == freeListSwap[j])
		{
			freeListSwap[j] = -1;
			swapAllocated--;
		}
	}
	#if 0
	printf("* --------------------------------------\n");
	printf("* Free List  : ");
	for(i = 0; i < numSwapPages; i++)
	{
		if(-1 == freeListSwap[i]) printf("%2d ", i);
	}
	printf("\n* --------------------------------------\n");
	#endif
  return (mNormal);
}

//#define GSAD
mType* get_swap_address(int pid, int offset)
{
        int pageNum, dOffset;
	int iOffset;
        int* offsetPtr;
        int* pageTableLocal = NULL;

	pageTableLocal = pageTablePtr[pid];
	pageNum = offset >> pageOffsetBits;
	iOffset = offset & ((pageSize * sizeof(int)) - 1);

	if(pid == freeListSwap[pageTableLocal[pageNum]])
	{
		offsetPtr = &swapPageFrame[(pageTableLocal[pageNum] * pageSize)];
		#ifdef GSAD
		printf("* #Page: %d, #Frame: %d, Pid: %d, MemPtr: %p, CurPtr: %p, CurOffPtr: %p\n", 
				pageNum, pageTableLocal[pageNum], freeListSwap[pageTableLocal[pageNum]], 
				swapPageFrame, offsetPtr, offsetPtr + iOffset);
		#endif
		return (mType*)(offsetPtr + iOffset);
	}
        return NULL;
}

void dump_swap_memory (pid)
int pid;
{ 
	int i;
	int offset, dataOffset;
	mType *start;
	int *pageTab;
	int mSize = 0;
	int numPages = 0;

	#if 0
        printf("* --------------------------------------\n");
        printf("* Free List  : ");
        for(i = 0; i < numSwapPages; i++)
        {
                if(-1 == freeListSwap[i]) printf("%2d ", i);
        }
        printf("\n");
	#endif

	pageTab = pageTablePtr[pid];
	mSize = (PCB[pid]->Mbound + 1);
	numPages = mSize/pageSize;
	if(0 != mSize%pageSize) numPages++;
        printf("* --------------------------------------\n");
	printf("* Swap Page Table for Process: %d\n", pid);
        printf("* --------------------------------------\n");
	for (i=0; i < numPages; i++)
	{
		printf("* Page: %d -> Frame: %d\n", i, pageTab[i]);
	}

        printf("* --------------------------------------\n");
	printf("* Swap Instruction Memory Dump for Process: %d\n", pid);
        printf("* --------------------------------------\n");
	for (i=0; i<PCB[pid]->numInstr; i++)
	{
		offset = ((i/pageSize) << pageOffsetBits) | (i%pageSize);
		start = (mType*)get_swap_address(pid, offset);
		printf ("* Page %d : Addr: %p -> Instr: %x\n", i/pageSize, start, start->mInstr);
	}
	printf ("\n");

        printf("* --------------------------------------\n");
	printf("* Swap Data Memory Dump for Process: %d \n", pid);
        printf("* --------------------------------------\n");
	for (i=0; i<PCB[pid]->numStaticData; i++)
	{
		dataOffset = ((PCB[pid]->MDbase >> pageOffsetBits) * pageSize) + (PCB[pid]->MDbase & ((pageSize * sizeof(int)) - 1)) + i;
                dataOffset = (dataOffset/pageSize) << pageOffsetBits | (dataOffset%pageSize);
		start = (mType*)get_swap_address(pid, dataOffset);
		printf ("* Page %d : Addr: %p -> Data: %.2f\n", (PCB[pid]->numInstr + i)/pageSize, start, start->mData);
	}
	printf ("\n");
}

void dump_swap_free_list()
{
        int i;
        printf("* ----------------------------\n");
        printf("* Swap Space Free List \n");
        printf("* ----------------------------\n");
        for(i = 0; i < numSwapPages; i++)
        {
                if(-1 == freeListSwap[i]) printf("%2d ", i);
        }
        #if 0
        printf("\n");
        printf("* Page Frames: ");
        for(i = 0; i < numP; i++)
        {
                printf("%2d ", freeList[i]);
        }
        #endif
        printf("\n* --------------------------------------\n");
}


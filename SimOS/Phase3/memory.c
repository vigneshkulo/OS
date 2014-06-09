#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simos.h"

//======================================================================
// Our memory addressing is in WORDs, because of the Memory structure def
// - so all addressing is computed in words
//======================================================================

#define fixMsize 256  // each process has a fixed memory size
                      // first part of the process memory is for program
                      // remaining is for data
                      // this size is actually 256*4 = 1024B
#define dataSize 4    // each memroy unit is of 4 bytes
#define OSsize 256 // first physical memory segment is for OS
                   // so first user physical memory starts at 1024
#define DATA	0
#define INST	1

#define READ	0
#define WRITE	1
#define IDLE	2

#define WORDSIZE	32

/*--- Debug Defintions ---*/
#if 0
#define LDINS
#define LD
#define GI
#define PD
#define GD

//#define LDS2M
//#define GAD
//#define LDPG
//#define CA
#endif

int numPP = 0;
int allocated = 0;
unsigned char* ageVect;
int* dirtyBit;
int* freeList;

#define opcodeShift 24
#define operandMask 0x00ffffff

//============================
// Our memory implementation is a mix of memory manager and physical memory.
// get_instr, put_instr, get_data, put_data are the mix
//   for instr, instr is fetched into registers: IRopcode and IRoperand
//   for data, data is fetched into registers: MBR (need to retain AC value)
//             but stored directly from AC
//   -- one reason is because instr and data do not have the same types
//      also for convenience
// allocate_memory, deallocate_memory are pure memory manager activities
//============================


//==========================================
// run time memory access operations
// Note:
//==========================================

//==========================================
// load instructions and data into memory 
// a specific pid is needed for loading, since registers are not for this pid
//==========================================
int check_offset(int pid, int offset)
{
	if (offset > PCB[pid]->Mbound)
	{ 
		printf ("* Process %d accesses offset(%d). Outside [%d] addr space!\n", CPU.Pid, offset, CPU.Mbound);
		return (mError);
	}
	return (mNormal);
}

int check_address (maddr)
int maddr;
{ 
	int pageNum = 0;
	int frameNum = 0;
        int* pageTableLocal = NULL;

	#ifdef CHKADDR
	if (Debug) printf ("* Check Address: Memory Offset: %d; ", maddr);
	#endif

	pageNum = maddr/pageSize;
	pageTableLocal = PCB[CPU.Pid]->Mbase;
	frameNum = pageTableLocal[pageNum];

	if(1 != CPU.Pid)
	{
		if (frameNum < OSmemSize/pageSize)
		{ 
			printf ("* Process %d accesses offset(%d). In OS region!\n", CPU.Pid, maddr);
			return (mError);
		}
	}
	if (maddr > CPU.Mbound)
	{ 
		printf ("* Process %d accesses offset(%d). Outside addr space!\n", CPU.Pid, maddr);
		return (mError);
	}
	else if (frameNum >= memSize/pageSize) 
	{ 
		printf ("* Process %d accesses (%d). Outside memory!\n", CPU.Pid, maddr);
		return (mError);
	}
	else 
	{ 
		#ifdef CHKADDR
		if (Debug) printf ("content = %x, %.2f\n", maddr->mData, maddr->mInstr);
		#endif
		return (mNormal);
	}
}

int printMem(mType* mStart, mType* mDest)
{
	int i;
	printf("* Source Addr: %p, Destin Addr: %p\n", mStart, mDest);	
	printf("* ---------------- Source --------------------\n");
	for(i = 0; i < 8; i++)
		printf ("* %d: Content = %.2f, %x\n", i, mStart[i].mData, mStart[i].mInstr);
	printf("\n* ---------------- Destin --------------------\n");
	for(i = 0; i < 8; i++)
		printf ("* %d: Content = %.2f, %x\n", i, mDest[i].mData, mDest[i].mInstr);
	return mNormal;
}

void dump_age_vector()
{
	int i;
	printf("* ---------------------\n");
	printf("* Age Vector  : ");
        for(i = 0; i < memSize/pageSize; i++)
        {
                printf("%x ", ageVect[i]);
        }
        printf("\n");
	printf("* ---------------------\n");
}

int replace_page(int pgN, IOInfo* fltInfo)
{
	int i, loc;
	unsigned char small;
	int pid;
	int frameN;
	int *pageTab;
	mType* mStart;
	mType* mDest;
	int mSize, numPages;
        int* pageTableLocal = NULL;

        small = 0xFF;
	pageTableLocal = (int*)CPU.Mbase;

	/* Search for the Frame with smallest Age value */
        for(i = OSmemSize/pageSize; i < memSize/pageSize; i++)
        {
                if(ageVect[i] < small)
                {
                        small = ageVect[i];
			loc = i;
                }
        }
	printf(" Frame Allocated: %d \n", loc);
	fltInfo->frameNum = loc;

	frameN = loc;
	/* Update the Pagetable of the Process which currently owns this frame */
	pid = freeList[frameN];
	pageTab = PCB[pid]->Mbase;
	mSize = (PCB[pid]->Mbound + 1);
	numPages = mSize/pageSize;
	if(0 != mSize%pageSize) numPages++;
	for (i=0; i < numPages; i++)
	{
		if( frameN == pageTab[i])
		{
			printf("* Page: %d of Process %d had Frame: %d\n", i, pid, pageTab[i]);
			pageTab[i] = -1;
			break;
		}
	}

	/* These values are overwritten when Dirty Bit is Set */
	fltInfo->flag = SWAPIN;		
	fltInfo->swOutAddr = NULL;

	/* If dirty Bit, set the flag to SWAPINOUT */
	if(1 == dirtyBit[frameN]) 
	{
		fltInfo->flag = SWAPINOUT;
		dirtyBit[frameN] = 0;
		fltInfo->swOutAddr = (mType*) get_page_start_addr(pid, i);
	}
	/* Update the Freelist and Page table of the process which got a new frame allocated */
	freeList[loc] = CPU.Pid;
	pageTableLocal[pgN] = loc;

	return mNormal;	
}

int fillFaultInfo(int pgN, IOInfo* fltInfo)
{
	int i, j;
	mType* mStart;
	mType* mDest;
        int* pageTableLocal = NULL;

	pageTableLocal = (int*)CPU.Mbase;

	for(i = 1, j = 0; j < numP; j++)
	{
		if(-1 == freeList[j])
		{
			printf(" Frame Allocated: %d\n", j);
			freeList[j] = CPU.Pid;
			pageTableLocal[pgN] = j;
			fltInfo->flag = SWAPIN;
			fltInfo->swOutAddr = NULL;
			fltInfo->frameNum = j;
			allocated++;
			break;
		}
		else
		{
			i++;
			if(i == numP) replace_page(pgN, fltInfo);
		}
	}
	/* Address of Frame to copied from Swap Space */
	fltInfo->swInAddr = (mType*)get_page_start_addr(CPU.Pid, pgN);

	if(Observe)
		printf("* Age Vector of #Frame %d: %x -> ", pageTableLocal[pgN], ageVect[pageTableLocal[pgN]]);

	ageVect[pageTableLocal[pgN]] |= 0x80;

	if(Observe)
		printf("%x\n", ageVect[pageTableLocal[pgN]]);

	return (mNormal);
}

mType* compute_address(int offset, int type)
{
	int pageNum, dOffset;
        int* offsetPtr;
        int* pageTableLocal = NULL;

	pageNum = offset >> pageOffsetBits;
	dOffset = offset & ((pageSize * sizeof(int)) - 1);

	if(Observe)
		printf("* Compute Address: Offset: %d, ", (pageNum*pageSize)+dOffset); 

	if(IDLE == type) pageTableLocal = PCB[1]->Mbase;
	else pageTableLocal = (int*)CPU.Mbase;

	if(-1 == pageTableLocal[pageNum])
	{
		PCB[CPU.Pid]->pgFltN = pageNum; 
		if(Observe)
			printf("Physical Address Calculated: Page Fault Occured\n");

		return NULL;
	}

	if(CPU.Pid == freeList[pageTableLocal[pageNum]] || IDLE == type)
	{
		offsetPtr = &pageFrame[(pageTableLocal[pageNum] * pageSize)];

		if(Observe)
			printf("Physical Address Calculated: %p\n", offsetPtr + dOffset);

		if(IDLE == type || 1 == CPU.Pid) return (mType*)(offsetPtr + dOffset);

		if(Observe)
			printf("* Age Vector of #Frame %d: %x -> ", pageTableLocal[pageNum], ageVect[pageTableLocal[pageNum]]);

		ageVect[pageTableLocal[pageNum]] |= 0x80;

		if(Observe)
			printf("%x\n", ageVect[pageTableLocal[pageNum]]);

		if(WRITE == type)
		{
			if(Observe)
				printf("* Dirty Bit of #Frame %d: %d -> ", pageTableLocal[pageNum], dirtyBit[pageTableLocal[pageNum]]);

			dirtyBit[pageTableLocal[pageNum]] = 1;

			if(Observe)
				printf("%d\n", dirtyBit[pageTableLocal[pageNum]]);
		}

		return (mType*)(offsetPtr + dOffset);
	}
	return NULL;
}

int get_data (offset)
int offset;
{ 
	#ifdef GD
	printf("* Get Data: Offset: %d\n", offset);
	#endif


	mType* maddr; 
	int chkOffset;
	int dataOffset;

	dataOffset = ((CPU.MDbase >> pageOffsetBits) * pageSize) + (CPU.MDbase & ((pageSize * sizeof(int)) - 1)) + offset;
	chkOffset = dataOffset;
	if (check_offset (CPU.Pid, chkOffset) == mError) return (mError);
	dataOffset = (dataOffset/pageSize) << pageOffsetBits | (dataOffset%pageSize);

	#ifdef GD
	printf("* Get Data: Data Offset: %d\n", dataOffset);
	#endif
	maddr = compute_address(dataOffset, READ);
	if (NULL == maddr) 
	{
		return (mPFault);
	}
	if (check_address (chkOffset) == mError) return (mError);
	else
	{ 
		CPU.MBR = maddr->mData;
		#ifdef GD
		printf("* Get Data: Data retrieved: %f\n\n", CPU.MBR);
		#endif
		return (mNormal);
	}
}

int put_data (offset)
int offset;
{ 
	#ifdef PD
	printf("* Put Data: Offset: %d\n", offset);
	#endif

	mType* maddr; 
	int chkOffset;
	int dataOffset;

	dataOffset = ((CPU.MDbase >> pageOffsetBits) * pageSize) + (CPU.MDbase & ((pageSize * sizeof(int)) - 1)) + offset;
	chkOffset = dataOffset;
	if (check_offset (CPU.Pid, chkOffset) == mError) return (mError);
	dataOffset = (dataOffset/pageSize) << pageOffsetBits | (dataOffset%pageSize);

	maddr = compute_address(dataOffset, WRITE);
	if (check_address (chkOffset) == mError) return (mError);
	if (NULL == maddr) 
	{
		return (mPFault);
	}
	else
	{ 
		maddr->mData = CPU.AC;

		#ifdef PD
		printf("* Put Data: Data Written: %f, %f\n\n", maddr->mData, CPU.AC);
		#endif
		return (mNormal);
	}
}
int get_instruction (offset)
int offset;
{ 
	#ifdef GI
	printf("* Get Instruction: Pid: %d, Offset: %d\n", CPU.Pid, offset );
	#endif

	int instr; 
	mType* maddr;
	int chkOffset;

	chkOffset = offset;
	if (check_offset (CPU.Pid, chkOffset) == mError) return (mError);
	offset = ((offset/pageSize) << pageOffsetBits) | (offset%pageSize);
	maddr = compute_address(offset, READ);
	if (NULL == maddr) 
	{
		return (mPFault);
	}
	if (check_address (chkOffset) == mError) return (mError);
	else
	{ 
		instr = maddr->mInstr;
		CPU.IRopcode = instr >> opcodeShift; 
		CPU.IRoperand = instr & operandMask;
		#ifdef GI
		printf("* Get Instruction: Opcode: %d, Operand: %d\n\n", CPU.IRopcode, CPU.IRoperand);
		#endif
		return (mNormal);
	}
}

int load_instruction (pid, offset, opcode, operand)
int pid, offset, opcode, operand;
{ 
	#ifdef LDINS
	printf("* Load Instruction: %d\n", pid);
	#endif

	mType* maddr;
	
	/* Check Offset is Valid */
	if (check_offset (pid, offset) == mError) return (mError);

	offset = ((offset/pageSize) << pageOffsetBits) | (offset%pageSize);

	/* Idle Pid is inside OS. And always resides in Memory */
	if(1 == pid)
	{
		maddr = (mType*)compute_address(offset, IDLE);
	}
	else
	{
		maddr = get_swap_address(pid, offset);
	}
	#ifdef LDINS
	printf("* Memory Address: %p\n", maddr);
	#endif

	if (NULL == maddr) return (mError);
	else
	{ 
		#ifdef LDINS
		printf("* Opcode: %d, Operand: %d\n", opcode, operand);
		#endif
		opcode = opcode << opcodeShift;
		operand = operand & operandMask;
		maddr->mInstr = opcode | operand;	
		#ifdef LDINS
		printf("* Memory Value: %x\n\n", maddr->mInstr);
		#endif
		return (mNormal);
	}
}

int load_swap_to_mem(int pid)
{
	int j;
	int numP;
	mType* mStart;
	mType* mDest;
        int* pageTableLocal = NULL;
	
	#ifdef LDS2M
	printf("* Loading Page 0 of Process: %d\n", pid);	
	#endif

	mStart = (mType*)get_page_start_addr(pid, 0);
	if(NULL == mStart) 
	{
		printf("* Error Loading: No Swap Space Memory for Process: %d\n", pid);
		return mError;
	}

	pageTableLocal = PCB[pid]->Mbase;
	numP = memSize/pageSize;

	if(1 != pid)
	{
	//	printf("* #Frame Allocated:");
		for(j = 0; j < numP; j++)
		{
			if(-1 == freeList[j])
			{
	//			printf(" %d ", j);
				freeList[j] = pid;
				pageTableLocal[0] = j;
				allocated++;
				break;
			}
		}
	}
	if(pid == freeList[pageTableLocal[0]])
	{
		mDest = (mType*)&pageFrame[(pageTableLocal[0] * pageSize)];

		memcpy(mDest, mStart, pageSize*sizeof(int));

		if(1 != pid)
		{
			if(Observe)
				printf("* Age Vector of #Frame %d: %x -> ", pageTableLocal[0], ageVect[pageTableLocal[0]]);

			ageVect[pageTableLocal[0]] |= 0x80;

			if(Observe)
				printf("%x\n", ageVect[pageTableLocal[0]]);
		}	
		return mNormal;
	}
}

int load_data (pid, offset, data)
int pid, offset;
float data;
{ 
	#ifdef LD
	printf("* Load Data\n");
	#endif

	mType *maddr;
	int dataOffset;

	dataOffset = ((PCB[pid]->MDbase >> pageOffsetBits) * pageSize) + (PCB[pid]->MDbase & ((pageSize * sizeof(int)) - 1)) + offset;

	if (check_offset (pid, dataOffset) == mError) return (mError);

	dataOffset = (dataOffset/pageSize) << pageOffsetBits | (dataOffset%pageSize);
	if(1 == pid)
	{
		maddr = (mType*)compute_address(dataOffset, IDLE);
	}
	else
	{
		maddr = (mType*)get_swap_address(pid, dataOffset);
	}
	if (NULL == maddr) return (mError);
	else
	{ 
		maddr[0].mData = data;	
		#ifdef LD
		printf("* Memory Value: %f\n\n", maddr[0].mData);
		#endif
		return (mNormal);
	}
}


//==========================================
// memory management functions -- to be implemented for paging

void memory_agescan ()
{
	int i;
	if(Observe)
	{
		printf("* ---------------------\n");
		printf ("* Age Scan Before: ");
		for(i = 0; i < memSize/pageSize; i++)
			printf("%x ", ageVect[i]);
		printf("\n");
	}
        for(i = OSmemSize/pageSize; i < memSize/pageSize; i++)
        {
               	ageVect[i] >>= 1; 
		if(0 == ageVect[i] && -1 != freeList[i]) 
		{
			// Do Nothing. Page Will be Copied back Later.
		}
        }
	if(Observe)
	{
		printf ("* Age Scan After : ");
		for(i = 0; i < memSize/pageSize; i++)
			printf("%x ", ageVect[i]);
		printf("\n");
		printf("* ---------------------\n");
	}
}

void init_pagefault_handler (pid)
{
	IOInfo* faultInfo;
	faultInfo = (IOInfo*) malloc (sizeof(IOInfo));

	printf("\n* ---------------------- ************** -----------------------\n");
	printf("* Page Fault for Process: %d, Page: %d,", pid, PCB[pid]->pgFltN);
	
	faultInfo->pid = pid;
	fillFaultInfo(PCB[pid]->pgFltN, faultInfo);
	printf("* ---------------------- ************** -----------------------\n");

	#if 0
	printf ("* Pid		: %d\n", faultInfo->pid);
	printf ("* Flag		: %d\n", faultInfo->flag);
	printf ("* Frame Num	: %d\n", faultInfo->frameNum);
	printf ("* Swap In	: %p\n", faultInfo->swInAddr);
	printf ("* Swap Out	: %p\n", faultInfo->swOutAddr);
	#endif

	insert_IOrequest(faultInfo);
	sem_post(&activateIO);
}

void pagefault_complete (pid)
{
//	printf("\n* Page Fault Handling Completed\n"); 
	insert_doneWait_process (pid);
	set_interrupt (doneWaitInterrupt);
}

//==========================================
// memory management functions -- general
//#define BITS_RQ
int bits_reqd( int num )
{
	if (num == 0)	return(0);	
	#ifdef BITS_RQ
        printf("* -------------------------------------------------------\n");
	#endif
	int i;
	#ifdef BITS_RQ
	printf("* Bits Required: Num   : %d \n", num);
	#endif
	unsigned int Num = abs(num);
	for ( i = 1; Num > 0; ++i ) {
		Num >>=1;
		#ifdef BITS_RQ
		printf("* Bits Required: Shifting >>1 : %d \n", Num);
		#endif
	}
	#ifdef BITS_RQ
	printf("* Bits Required: Return: %d \n", i);
        printf("* -------------------------------------------------------\n");
	#endif
	return( i );
}

void initialize_memory ()
{ 
	initialize_swap_memory();
	int i;
        numP = memSize/pageSize;
        printf("* --------------------------------------\n");
        printf("* Initializing Memory\n");
        printf("* --------------------------------------\n");
	pageOffsetBits = bits_reqd(pageSize);	
	printf("* Bits Required: %d\n", pageOffsetBits);
        printf("* No. of Pages (%d / %d): %d\n", memSize, pageSize, numP);

        ageVect = (unsigned char*) malloc(numP * sizeof(unsigned char));
        dirtyBit = (int*) malloc(numP * sizeof(int));
        freeList = (int*) malloc(numP * sizeof(int));

	printf("* Free List  : ");
        for(i = 0; i < numP; i++)
        {
                freeList[i] = -1;
		dirtyBit[i] = 0;
		ageVect[i] = 0;
                printf("%2d ", i);
        }
        printf("\n");
        printf("* Page Frames: ");
        for(i = 0; i < numP; i++)
        {
                printf("%2d ", freeList[i]);
        }
        printf("\n\n");
        pageFrame = (int*)malloc(memSize * pageSize * sizeof(int));

	add_timer (periodAgeScan, osPid, actAgeInterrupt, periodAgeScan);
	// in demand paging, some more initialization is probably needed
}

int allocate_memory (pid, msize, numinstr)
int pid, msize, numinstr;
{
	int *pageTab;
	int i, j, pageNum;
	if(1 < pid)
	{
		if(-2 == allocate_swap_memory (pid, msize, numinstr))
		{
			printf ("* Swap (or) Pid Limit Reached\n");
			printf("* --------------------------------------\n");
			free_PCB(pid);
			return mPageUnavail;
		}

		printf("* --------------------------------------\n");
		printf("* Allocate Memory: %d words by Process: %d\n", msize, pid);
		printf("* --------------------------------------\n");

		numPP = msize/pageSize;
		if(0 != msize%pageSize) numPP++;

		printf("* #Pages Requested : %d\n", numPP);
		pageTab = (int*) malloc(numPP * sizeof(int));
		for(i = 0; i < numPP; i++)
		{
			pageTab[i] = -1;
		}
		#if 0
		printf("* #Frames Allocated:");
		for(j = 0; j < numP; j++)
		{
			if(-1 == freeList[j])
			{
				printf(" %d ", j);
				freeList[j] = pid;
				pageTab[0] = j;
				allocated++;
				break;
			}
		}
		#endif
	}
	else if(0 == pid)
	{
		numPP = msize/pageSize;
		if(0 != msize%pageSize) numPP++;

		if(numPP > (numP - allocated))
		{
			printf("* Pages unavailable\n");
			printf("* --------------------------------------\n");
			return(mPageUnavail);                                                
		}
		printf("* #Pages Requested : %d\n", numPP);
		pageTab = (int*) malloc(numPP * sizeof(int));
		printf("* #Frames Allocated:");
		for(i = 0; i < numPP; i++)
		{
			for(j = 0; j < numP; j++)
			{
				if(-1 == freeList[j])
				{
					printf(" %d ", j);
					freeList[j] = pid;
					pageTab[i] = j;
					allocated++;
					break;
				}
			}
		}
	}
	else if (1 == pid)
	{
		#if 0
		if(-2 == allocate_swap_memory (pid, msize, numinstr))
		{
			printf ("* Swap (or) Pid Limit Reached\n");
			printf("* --------------------------------------\n");
			free_PCB(pid);
			return mPageUnavail;
		}
		#endif

		printf("* --------------------------------------\n");
		printf("* Allocate Memory: %d words by Process: %d\n", msize, pid);
		printf("* --------------------------------------\n");

		numPP = msize/pageSize;
		if(0 != msize%pageSize) numPP++;

		if(numPP > OSmemSize/pageSize)
		{
			printf("* Pages unavailable\n");
			printf("* --------------------------------------\n");
			return(mPageUnavail);                                                
		}
		printf("* #Pages Requested : %d\n", numPP);
		pageTab = (int*) malloc(numPP * sizeof(int));
		printf("* #Frames Allocated:");
		for(i = 0; i < numPP; i++)
		{
			for(j = 0; j < OSmemSize/pageSize; j++)
			{
				if(0 == freeList[j])
				{
					printf(" %d ", j);
					freeList[j] = pid;
					pageTab[i] = j;
					allocated++;
					break;
				}
			}
		}
	}
	printf("\n");
	PCB[pid]->Mbase = pageTab;
	PCB[pid]->Mbound = msize - 1;
	pageNum = numinstr/pageSize;
	pageNum = pageNum << pageOffsetBits;
	PCB[pid]->MDbase = 0;
	PCB[pid]->MDbase = pageNum | (numinstr%pageSize);

	#if 0
	printf("* --------------------------------------\n");
	printf("* Free List  : ");
	for(i = 0; i < numP; i++)
	{
		if(-1 == freeList[i]) printf("%2d ", i);
	}
	printf("\n");
	printf("* Page Frames: ");
	for(i = 0; i < numP; i++)
	{
		printf("%2d ", freeList[i]);
	}
	printf("\n* --------------------------------------\n");
	#endif
	return (mNormal);
}

int free_memory (pid)
int pid;
{
	int i, j;
	int *pageTab;
	pageTab = PCB[pid]->Mbase;
	if(NULL != pageTab) free(pageTab);

	for(j = 0; j < numP; j++)
	{
		if(pid == freeList[j])
		{
			freeList[j] = -1;
			ageVect[j] = 0;
			dirtyBit[j] = 0;
			allocated--;
		}
	}
	printf("* --------------------------------------\n");
	printf("* Free List  : ");
	for(i = 0; i < numP; i++)
	{
		if(-1 == freeList[i]) printf("%2d ", i);
	}
	printf("\n");
	printf("* Page Frames: ");
	for(i = 0; i < numP; i++)
	{
		printf("%2d ", freeList[i]);
	}
	printf("\n* --------------------------------------\n");
  return (mNormal);
}

mType* get_address(int pid, int offset)
{
	int pageNum, dOffset, iOffset;
        int* offsetPtr;
        int* pageTableLocal = NULL;

	pageTableLocal = PCB[pid]->Mbase;
	pageNum = offset >> pageOffsetBits;
	iOffset = offset & ((pageSize * sizeof(int)) - 1);
	#ifdef GAD
	printf("* PT [Mbase] -> %p, #Page: %d, #Frame: %d, Pid: %d, Ptr: %p\n", pageTableLocal, pageNum, pageTableLocal[pageNum], 
					freeList[pageTableLocal[pageNum]], &pageFrame[(pageTableLocal[pageNum] * pageSize)]);
	#endif
	if(pid == freeList[pageTableLocal[pageNum]])
	{
		offsetPtr = &pageFrame[(pageTableLocal[pageNum] * pageSize)];
		return (mType*)(offsetPtr + iOffset);
	}

	return NULL;
}

void dump_memory (pid)
int pid;
{ 
	int i, offset;
	mType *start;
	int *pageTab;
	int mSize = 0;
	int dataOffset;
	int numPages = 0;

	pageTab = PCB[pid]->Mbase;
	mSize = (PCB[pid]->Mbound + 1);
	numPages = mSize/pageSize;
	if(0 != mSize%pageSize) numPages++;
        printf("* --------------------------------------\n");
	printf("* Page Table for Process: %d\n", pid);
        printf("* --------------------------------------\n");
	for (i=0; i < numPages; i++)
	{
		if(-1 == pageTab[i])
			printf("  Page: %d -> Frame: Not  Allocated\n", i);
		else
			printf("  Page: %d -> Frame: %d\n", i, pageTab[i]);
	}

        printf("* --------------------------------------\n");
	printf("* Instruction Memory Dump for Process: %d\n", pid);
        printf("* --------------------------------------\n");
	for (i=0; i < PCB[pid]->numInstr; i++)
	{
		offset = ((i/pageSize) << pageOffsetBits) | (i%pageSize);
		start = get_address(pid, offset);
		if(NULL == start)
		{
		//	printf("  Page %d : %p -> Not in Memory\n", i/pageSize, start);
		}
		else
			printf ("  Page %d, Age %d, Dirty Bit %d, Addr: %p -> Instr: %x\n", i/pageSize, ageVect[pageTab[i/pageSize]], 
											dirtyBit[pageTab[i/pageSize]], start, start->mInstr);
	}

        printf("* --------------------------------------\n");
	printf("* Data Memory Dump for Process: %d \n", pid);
        printf("* --------------------------------------\n");
	for (i=0; i<PCB[pid]->numStaticData; i++)
	{

		dataOffset = ((PCB[pid]->MDbase >> pageOffsetBits) * pageSize) + (PCB[pid]->MDbase & ((pageSize * sizeof(int)) - 1)) + i;
		dataOffset = (dataOffset/pageSize) << pageOffsetBits | (dataOffset%pageSize);
		start = get_address(pid, dataOffset);
		if(NULL == start)
		{
		//	printf("  Page %d : %p -> Not in Memory\n", (PCB[pid]->numInstr + i)/pageSize, start);
		}
		else
			printf ("  Page %d, Age %d, Dirty Bit %d, Addr: %p -> Data: %.2f\n", (PCB[pid]->numInstr + i)/pageSize, ageVect[pageTab[i/pageSize]],
											dirtyBit[pageTab[i/pageSize]], start, start->mData);
	}
	printf ("\n");
}

void dump_dirty_bit()
{
	int i;
	printf("* --------------------------------------\n");
	printf("* Dirty Bit  : ");
        for(i = 0; i < memSize/pageSize; i++)
        {
                printf("%d ", dirtyBit[i]);
        }
        printf("\n");
	printf("* --------------------------------------\n");
}

void dump_free_list()
{
	int i;
	printf("* --------------------------------------\n");
	printf("* Free List  : ");
	for(i = 0; i < memSize/pageSize; i++)
	{
		if(-1 == freeList[i]) printf("%2d ", i);
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

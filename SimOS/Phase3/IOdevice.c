#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "simos.h"

#define MAX_REQ 100

IOInfo* IOrequestList[MAX_REQ];
int IOrequestCount = 0;
int IOrequestHead = 0;
int IOrequestTail = 0;

void insert_IOrequest (IOInfo* reqInfo)
{ 
	if (IOrequestCount >= MAX_REQ)
	{ 
		printf ("* Request queue is Full\n"); 
	}
	else 
	{ 
		IOrequestList[IOrequestTail] = reqInfo;
		IOrequestCount++;
		IOrequestTail = (IOrequestTail+1) % MAX_REQ;
	}
}

IOInfo* get_IOrequest ()
{
	while (IOrequestCount == 0);
	return (IOrequestList[IOrequestHead]);
}

void remove_IOrequest ()
{
	if (IOrequestCount == 0)
	{ 
		printf ("* Incorrect Removal of IOrequest list!!!\n"); 
		exit(-1); 
	}
	IOrequestCount--;
	IOrequestHead = (IOrequestHead+1) % MAX_REQ;
}

void dump_IO()
{ 
	int i, next;
	IOInfo* req;

	printf ("* ----------------------------------------------- \n");
	printf ("* IO Request Queue Dump\n");
	printf ("* ----------------------------------------------- \n");
	printf ("* Count: %d; Head: %d; Tail: %d\n", IOrequestCount, IOrequestHead, IOrequestTail);
	for (i=0; i<IOrequestCount; i++)
	{ 
		next = (IOrequestHead+i) % MAX_REQ;
		req = IOrequestList[next];
		printf ("* IORequest[%d]: Pid: %d, Frame: %d, Flag: %s, Swap In Addr: %p, Swap Out Addr: %p\n", 
				next, req->pid, req->frameNum, (1 == req->flag) ? "SWAPIN" : ((2 == req->flag) ? "SWAPINOUT":""),
				req->swInAddr, req->swOutAddr);
	}
	printf ("* ----------------------------------------------- \n");
}

void IOstart()
{
	int pid;
	mType* mDest = NULL;
	mType* mStart = NULL;
	IOInfo* localInfo;
	
	while(1)
	{
		sem_wait(&activateIO);
		localInfo = get_IOrequest();

		if(Observe)
		{
			printf ("* IO Thread Processing: Pid: %d, Frame: %d, Flag: %s, Swap In Addr: %p, Swap Out Addr: %p\n",
                                localInfo->pid, localInfo->frameNum, (1 == localInfo->flag) ? "SWAPIN" : ((2 == localInfo->flag) ? "SWAPINOUT":""),
                                localInfo->swInAddr, localInfo->swOutAddr);
		}
		if(SWAPIN == localInfo->flag)		
		{
			pid = localInfo->pid;
			mDest = (mType*)&pageFrame[(localInfo->frameNum * pageSize)];

			#if 0
			printf("* Page Swapping IN\n");
			printMem(localInfo->swInAddr, mDest);
			#endif

			memcpy(mDest, localInfo->swInAddr, pageSize*sizeof(int));

			#if 0
			printf("* Page Swapped IN\n");
			printMem(localInfo->swInAddr, mDest);
			#endif

			remove_IOrequest();
			pagefault_complete(pid);
			free(localInfo);
			localInfo = NULL;
			mDest = NULL;
		}
		else if (SWAPINOUT == localInfo->flag)
		{
			/* Write Back to Swap space */
			mStart = (mType*)&pageFrame[(localInfo->frameNum * pageSize)];

			#if 0
			printf("* Writing Back\n");
			printMem(mStart, localInfo->swOutAddr );
			#endif

			memcpy(localInfo->swOutAddr, mStart, pageSize*sizeof(int));
 
			#if 0
			printf("* Written Back\n");
			printMem(mStart, localInfo->swOutAddr );
			#endif

 
			#if 0
			printf("* Copying Back\n");
			printMem(localInfo->swInAddr, mStart);
			#endif

			/* Copy data from Swap space to Memory Frame */
			memcpy(mStart, localInfo->swInAddr, pageSize*sizeof(int));
 
			#if 0
			printf("* Copied Back\n");
			printMem(localInfo->swInAddr, mStart);
			#endif

			remove_IOrequest();
			pagefault_complete(pid);
			free(localInfo);
			localInfo = NULL;
			mStart = NULL;
		}
	}
}

void initialize_IO_device()
{
        printf("* --------------------------------------\n");
        printf("* Initializing IO Thread\n");
        printf("* --------------------------------------\n");

	pthread_t IOthread;
        sem_init(&activateIO, 0, 0);
	pthread_create(&IOthread, NULL, IOstart, NULL);
} 

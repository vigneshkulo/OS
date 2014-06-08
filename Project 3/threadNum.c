#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<math.h>
#include<semaphore.h>
#include<sys/syscall.h>

//#define DEBUG

/* Semaphores Used */
sem_t semStr;
sem_t semEnd;
sem_t semEndCnt;
sem_t semStrCnt;

/* Global Variables */
int N = 0;
int obsMode = 0;
int srtArray[10000]; 

int sortArray(void* args)
{
	int* tPtr = (int*)args;

	int i = 0;
	int tNo = *tPtr;
	int stage, phase;
	int noOfGrps, grpSize, g, gIndex, grpStart, grpEnd, index1, index2;

	for(stage = 0; stage < log2(N); stage++)
	{
		for(phase = 1; phase <= log2(N); phase++)
		{ 
			if(-1 == sem_post(&semStrCnt))
			{
				perror("Error in sem_post: ");
				fflush(stdout);
			}

			#ifdef DEBUG
			printf("* Thread %4d: Stage: %d, Phase: %d, Reached Start\n", tNo, stage, phase);
			fflush(stdout);
			#endif

			if(-1 == sem_wait(&semStr))
			{
				perror("Error in sem_wait: ");
				fflush(stdout);
			}
			noOfGrps = pow(2, (phase-1));
			grpSize = N / noOfGrps;
			g = tNo / (grpSize / 2);
			gIndex = tNo % (grpSize / 2);
			grpStart = g * grpSize;
			grpEnd = (g + 1) * grpSize - 1;
			index1 = grpStart + gIndex;
			index2 = grpEnd - gIndex;
			if(srtArray[index1] > srtArray[index2])
			{
				srtArray[index1] -= srtArray[index2]; 
				srtArray[index2] += srtArray[index1]; 
				srtArray[index1] =  srtArray[index2] - srtArray[index1]; 
			}
			for(i = 0; i < srtArray[index1] + srtArray[index2]; i++);

			if(-1 == sem_post(&semEndCnt))
			{
				perror("Error in sem_post: ");
				fflush(stdout);
			}

			#ifdef DEBUG
			printf("* Thread %4d: Stage: %d, Phase: %d, Reached End\n", tNo, stage, phase);
			fflush(stdout);
			#endif

			if(-1 == sem_wait(&semEnd))
			{
				perror("Error in sem_wait: ");
				fflush(stdout);
			}

			if(1 == obsMode)
			{
				printf("* Thread %4d: Stage: %d, Phase: %d, Current List: ", tNo, stage, phase);
				fflush(stdout);
				for(i = 0; i < N; i++)
				{
					printf(" %d", srtArray[i]);
					fflush(stdout);
					if(N-1 == i) printf(".");
					else printf(",");
					fflush(stdout);
				}
				printf("\n");
				fflush(stdout);
			}
		}
	}
	return 0;
}

int main(int argc, char* argv[])
{
	int i = 0;
	int num = 0;	
	int value = 0; 
	int index = 0;	
	int stageP, phaseP;
	unsigned int semVal1 = 0;
	unsigned int semVal2 = 0;
	unsigned int semVal3 = 0;
	unsigned int semVal4 = 0;
	int noOfGrpsP, grpSizeP, gP, gIndexP, grpStartP, grpEndP, index1P, index2P;

	FILE* fp = NULL;

	char mode[5];
	char line[50];
	char fileName[50];

	if(argc != 3)
	{
		printf("* Format is: executable input.dat -mode\n");
		exit(0);
	}

        printf("* --------------------------------------\n");
        if(NULL != argv[1])
        {
                strcpy(fileName, argv[1]);
                printf("* File Name  : %s\n", fileName);
        }
        if(NULL != argv[2])
        {
                strcpy(mode, argv[2]);
		if(!strncmp("-r", mode, 2))
		{
			printf("* Regular Mode\n");
		}	
		else if(!strncmp("-o", mode, 2))
		{
			printf("* Observation Mode\n");
			obsMode = 1;
		}
		else
		{
			printf("* -------------------\n");
			printf("* Error: Invalid Mode\n");
			printf("* -------------------\n");
			exit(0);
		}
        }

	fp = fopen( "semainit.dat", "r+");
	if(NULL == fp)
	{
		printf("* Sort  : Unable to Open File\n");
		exit(0);
	}

	if(NULL == fgets(line, 40, fp))
	{
		perror("* Sort  : ");
		exit(0);
	}
	sscanf(line, "%*d %d %d %d %d", &semVal1, &semVal2, &semVal3, &semVal4);

	fclose(fp);
	fp = NULL;

	printf("* Sem Init Values: %d, %d, %d, %d\n", semVal1, semVal2, semVal3, semVal4);
	sem_init(&semStr, 0, semVal1);
	sem_init(&semEnd, 0, semVal2);
	sem_init(&semStrCnt, 0, semVal3);
	sem_init(&semEndCnt, 0, semVal4);

	fp = fopen( fileName, "r+");
	if(NULL == fp)
	{
		printf("* Sort  : Unable to Open File\n");
		exit(0);
	}

	printf("* -------------------------------------------------------------------\n");
	while(NULL != fgets(line, 11, fp))
	{
		sscanf(line, "%d", &num);
		if(0 == num) break;

		N = num;
		printf("* Num in List: %d\n", N);
		printf("* Num of Threads: %d\n", (N/2) - 1);
		printf("* Stages: %.0lf, Phases: %.0lf\n", log2(N), log2(N));

		printf("* -------------------------------------------------------------------\n");
		printf("* Input Array: ");
		fflush(stdout);
		while(num)
		{
			fgets(line, 15, fp);
			sscanf(line, "%d", &srtArray[index]);
			printf(" %d", srtArray[index]);
			index++;
			num--;
			if(0 == num) printf(".");
			else printf(",");
		}
		index = 0;
		printf("\n");

		pthread_t thread[N/2];
		int tNo[N/2];
		int loop = 0;

		for(i = 1; i < N/2; i++)
		{
			tNo[i] = i;
			pthread_create(&thread[i], NULL, sortArray, (void*)&tNo[i]);
		}

		for(stageP = 0; stageP < log2(N); stageP++)
		{
			for(phaseP = 1; phaseP <= log2(N); phaseP++)
			{ 
				#ifdef DEBUG
				printf("* Main Thread: Stage: %d, Phase: %d, Reached Start \n", stageP, phaseP);
				fflush(stdout);
				#endif

				while((N/2-1) != value)
				{
					sem_getvalue(&semStrCnt, &value); 
				}

				#ifdef DEBUG
				printf("* ------------------------------------------------\n");
				fflush(stdout);
				printf("* %d: All threads have reached Start: SemStr [%d]\n", loop, value);
				printf("* ------------------------------------------------\n");
				fflush(stdout);
				#endif

				sem_getvalue(&semStr, &value); 
				while(0 != value)
				{
					sem_getvalue(&semStr, &value); 
				}

				for(i = 0; i < (N/2) -1 ; i++)
				{
					if(-1 == sem_post(&semStr))
					{
						perror("Error in sem_post: ");
						fflush(stdout);
					}
					if(-1 == sem_wait(&semStrCnt))
					{
						perror("Error in sem_wait: ");
						fflush(stdout);
					}
				}

				noOfGrpsP = pow(2, (phaseP-1));
				grpSizeP = N / noOfGrpsP;
				gP = 0 / (grpSizeP / 2);
				gIndexP = 0 % (grpSizeP / 2);
				grpStartP = gP * grpSizeP;
				grpEndP = (gP + 1) * grpSizeP - 1;
				index1P = grpStartP + gIndexP;
				index2P = grpEndP - gIndexP;
				if(srtArray[index1P] > srtArray[index2P])
				{
					srtArray[index1P] -= srtArray[index2P]; 
					srtArray[index2P] += srtArray[index1P]; 
					srtArray[index1P] =  srtArray[index2P] - srtArray[index1P]; 
				}
				for(i = 0; i < srtArray[index1P] + srtArray[index2P]; i++);

				#ifdef DEBUG
				printf("* Main Thread: Stage: %d, Phase: %d, Reached End\n", stageP, phaseP);
				fflush(stdout);
				#endif

				if(1 == obsMode)
				{
					printf("* Main Thread: Stage: %d, Phase: %d, Current List: ", stageP, phaseP);
					fflush(stdout);
					for(i = 0; i < N; i++)
					{
						printf(" %d", srtArray[i]);
						fflush(stdout);
						if(N-1 == i) printf(".");
						else printf(",");
						fflush(stdout);
					}
					printf("\n");
					fflush(stdout);
				}

				sem_getvalue(&semEndCnt, &value); 
				while((N/2-1) != value)
				{
					sem_getvalue(&semEndCnt, &value); 
				}

				sem_getvalue(&semEnd, &value); 
				while(0 != value)
				{
					sem_getvalue(&semEnd, &value); 
				}

				#ifdef DEBUG
				printf("* ------------------------------------------------\n");
				fflush(stdout);
				printf("* %d: All threads have reached End: SemEnd [%d]\n", loop, value);
				printf("* ------------------------------------------------\n");
				fflush(stdout);
				#endif

				for(i = 0; i < (N/2) - 1; i++)
				{
					if(-1 == sem_post(&semEnd))
					{
						perror("Error in sem_post: ");
						fflush(stdout);
					}
					if(-1 == sem_wait(&semEndCnt))
					{
						perror("Error in sem_wait: ");
						fflush(stdout);
					}
				}
				value = 0;
				loop++;
			}
		}
		for(i = 1; i < N/2; i++)
		{
			pthread_join(thread[i], NULL);
			#if 0
			printf("* Thread %d Exited\n", i);
			fflush(stdout);
			#endif
		}
		printf("* -------------------------------------------------------------------\n");
		printf("* Sorted List: ");
		fflush(stdout);
		for(i = 0; i < N; i++)
		{
			printf(" %d", srtArray[i]);
			fflush(stdout);
			srtArray[i] = 0;
			if(N-1 == i) printf(".");
			else printf(",");
			fflush(stdout);
		}
		printf("\n");
		printf("* -------------------------------------------------------------------\n\n");
	}
	return 0;
}


/*
 * Theatre Management System
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>

sem_t maxChk;
sem_t exitVal;
sem_t ttA, custTT;
sem_t finished[90];
sem_t agentA, custA;
sem_t cswA, custCSW;
sem_t mutex, mutex1, mutex2;

int max[5];
int count=0;
int ttQ = 0;
int cswQ = 0;
int seatN[5];
int agentQ[2];
int exitBA = 0;
int movieInx[90];
char movie[5][60];
int count_customers=0;

void* customer(void *arg)
{
	#if 1
	int *ptr;
	int custm;
	ptr = (int*) arg;
	custm = *ptr;

	int choice = rand()%5;
	movieInx[custm] = choice;

	sem_wait(&agentA);
	sem_post(&custA);

	sem_wait(&mutex);
	if (0 == agentQ[0]) agentQ[0] = custm;	
	else if (0 == agentQ[1]) agentQ[1] = custm;	
	sem_post(&mutex);

	printf("* Customer %d buying ticket to %s\n", custm, movie[choice]);
	fflush(stdout);
	sem_wait(&finished[custm]);

	if(99 != movieInx[custm])
	{
		sem_wait(&ttA);
		sem_post(&custTT);
		printf("* Customer %d in line to see ticket taker\n", custm);
		fflush(stdout);

		sem_wait(&mutex1);
		ttQ = custm;	
		sem_post(&mutex1);

		sem_wait(&finished[custm]);

		if(0 == custm%2)
		{
			printf("* Customer %d in line to buy Popcorn\n", custm);
			fflush(stdout);
			sem_wait(&cswA);
			sem_post(&custCSW);
			int choice1 = rand()%3;

			sem_wait(&mutex2);
			cswQ = custm | choice1<<16;	
			sem_post(&mutex2);

			sem_wait(&finished[custm]);
		}
		printf("* Customer %d enters theater to see %s\n", custm, movie[choice]);
		fflush(stdout);
	}
	else
	{
		printf("* Customer %d exits since '%s' tickets were FULL\n", custm, movie[choice]);
		fflush(stdout);
	}
	#endif
	#if 0
	sem_wait(&exitVal);
	count_customers++;
	printf("* Customer Count: %d\n", count_customers);
	fflush(stdout);
	sem_post(&exitVal);
	#endif
	printf("* Customer %d exits\n", custm);
	return NULL;
}

int counter=0;
void *boxOfficeAgents( void* arg)
{
	#if 1
	int *ptr;
	int cus = 0;
	int mov = 0;
	int agent;
	ptr = (int*) arg;
	agent = *ptr;
	
	printf("* Box Office agent %d created\n", agent);
	fflush(stdout);
	while(1 != exitBA)
	{
		sem_wait(&custA);

		sem_wait(&mutex);
		if (0 != agentQ[0]) 
		{
			cus = agentQ[0];	
			agentQ[0] = 0;
		}
		else if (0 != agentQ[1]) 
		{
			cus = agentQ[1];	
			agentQ[1] = 0;
		}
		sem_post(&mutex);

		mov = movieInx[cus];

		printf("* Box Office agent %d serving customer %d\n", agent, cus);
		fflush(stdout);
	//	sleep(2);
		
		sem_wait(&maxChk);
		if(seatN[mov] < max[mov])
		{
			printf("* Box Office agent %d sold ticket for '%s' to customer %d\n", agent, movie[mov], cus );
			fflush(stdout);
			seatN[mov]++;
		}
		else if (seatN[mov] >= max[mov])
		{
			movieInx[cus] = 99;	
		}
		sem_post(&maxChk);

		sem_post(&finished[cus]);
		sem_post(&agentA);
	}
	printf("* Box Office Agent %d Exited\n", agent);
	fflush(stdout);
	#endif
	return NULL;
}

void *ticketTaker()
{
	#if 1
	int cust = 0;

	while(1 != exitBA)
	{
		sem_wait(&custTT);

		sem_wait(&mutex1);
		cust = ttQ;	
		ttQ = 0;
		sem_post(&mutex1);

		printf("* Ticket taken from %d \n", cust);
		fflush(stdout);
		sem_post(&finished[cust]);
		sem_post(&ttA);
	}
	printf("* Ticket taker Exited\n");
	fflush(stdout);
	#endif
	return NULL;
}

void *concessionStandWorker()
{
	#if 1
	int data = 0;
	int cust = 0;
	int choice = 0;

	while(1 != exitBA)
	{
		sem_wait(&custCSW);

		sem_wait(&mutex2);
		data = cswQ;
		cswQ = 0;
		sem_post(&mutex2);

		choice = data >>16;
		cust = data & 0x0000FFFF;

		if(0 == choice)
		{
			printf("* Order for Popcorn taken from customer %d\n", cust);
			fflush(stdout);
		}
		else if(1 == choice)
		{
			printf("* Order for Soda taken from customer %d\n", cust);
			fflush(stdout);
		}
		else if(2 == choice)
		{
			printf("* Order for Popcorn and Soda taken from customer %d\n", cust);
			fflush(stdout);
		}
		sem_post(&finished[cust]);
		sem_post(&cswA);
	}
	printf("* CSW Exited\n");
	fflush(stdout);
	#endif
	return NULL;
}


int main(int argc, char *argv[]) 
{
	int i = 0;
	char *maxPtr;
	int tN[100];
	pthread_t tid[100];

	char line[60];
	FILE *file = fopen ( "input", "r+" );
	while(NULL != fgets(line, 60, file))
	{
		maxPtr = strchr(line, '\t');
		strncpy(movie[i], line, maxPtr-line);
		movie[i][maxPtr-line] = '\0';
		printf("* Movie: %s\n", movie[i]);
		maxPtr++;
		sscanf(maxPtr, "%d", &max[i]);
		printf("* Max limit for Movie %d: %d\n", i, max[i]);
		i++;
	}
	printf("* --------------------------------------\n");

	seatN[0] = 0;
	seatN[1] = 0;
	seatN[2] = 0;
	seatN[3] = 0;
	seatN[4] = 0;

	agentQ[0] = 0;
	agentQ[1] = 0;

        sem_init(&agentA, 0, 2);
        sem_init(&custA, 0, 0);

        sem_init(&ttA, 0, 1);
        sem_init(&custTT, 0, 0);

        sem_init(&cswA, 0, 1);
        sem_init(&custCSW, 0, 0);

        sem_init(&mutex, 0, 1);
        sem_init(&mutex1, 0, 1);
        sem_init(&mutex2, 0, 1);
        sem_init(&maxChk, 0, 1);
        sem_init(&exitVal, 0, 1);

	for(i=0;i<90;i++)
	{
		sem_init(&finished[i], 0, 0);
	}

	for(i=0;i<90;i++)
	{
		tN[i] = i;
		pthread_create( &tid[i], NULL, customer, (void*)&tN[i] ); //Creating customer threads
	}

	tN[90] = 0;
	pthread_create( &tid[90], NULL, boxOfficeAgents, (void*)&tN[90] ); //Creating boxoffice agents
	tN[91] = 1;
	pthread_create( &tid[91], NULL, boxOfficeAgents, (void*)&tN[91] ); //Creating boxoffice agents

	pthread_create( &tid[92], NULL, ticketTaker, NULL ); //Creating ticketTaker

	pthread_create( &tid[93], NULL, concessionStandWorker, (void *) NULL ); //Creating concession stand worker

	
	#if 1
	for(i=0;i<94;i++)
	{
		printf("* Thread %d ...\n", i);
		fflush(stdout);
		pthread_join( tid[i], NULL ); //Joining all the threads
	}
	#endif
	printf("* All Customers left threater\n");
	fflush(stdout);
	
	return 0;
}

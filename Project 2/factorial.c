/*
 *	Submitted By	: Vignesh Kulothungan
 *	Subject		: Operating Systems - Project 2
 */

/* Header Files*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

int recvN = 0;
int compN = 0;
int maxN = 0;
int minN = 0;
int prntStop = 0;

/* Function Calculating Factorial and Factorial % Field */
int facmod(int n, int m)
{
	int i = 0;
	int fact = 1;
	for(i = n; i > 0; i--)
	{
		fact*=i;
		fact%=m;
	}
	return fact;
}

void usr1Handler(int sigNo)
{
	recvN++;
//	printf("* Factor : Received SIGUSR1 Signal [%d - %d = %d]\n", recvN, compN, recvN-compN);
	if((recvN - compN) >= maxN && 0 == prntStop) 
	{
		kill(getppid(), SIGUSR1);
//		printf("* Factor : Stopping Parent: Limit Reached\n");
		prntStop = 1;
	}
}

/* Main Function */
int main(int argc, char *argv[])
{
	signal(SIGUSR1, usr1Handler);
	printf("* Factor : SIGUSR1 Signal Registered [%d]\n", getpid());

	int num1 = 0;
	int num2 = 0;
	int factCount = 0;
	int status = 1;

	int sockFd;
	int ackFd;
        int clientLen = 0;
	int parentFd;
	int start = 1;
        struct hostent *server;
	struct sockaddr_in servAddr, clientAddr;

	int buffer, i;

	int portN = 0;
	int parPortN = 0;
	char line[50];
	char chkStr[5];
	char hostName[20];
	char *cPtr = NULL;
	FILE* fp = NULL;

	fp = fopen("config.dat", "r+");
	if(NULL == fp)  exit(-1);

	fgets(line, 50, fp);
        sscanf(line, "%s", chkStr);
        chkStr[5] = '\0';
        if(!strcmp(chkStr, "mainp"))
        {
		cPtr = line;
		cPtr += 6;
                sscanf(cPtr, "%*s %d", &parPortN);
        }
        printf("* Factor : Parent Port Number: %d\n", parPortN);

	fgets(line, 50, fp);
	fgets(line, 50, fp);

	sscanf(line, "%s", chkStr);
	chkStr[5] = '\0';

	if(!strcmp(chkStr, "facto"))
	{
		cPtr = line;
		cPtr += 6;
		sscanf(cPtr, "%s", hostName);
		cPtr += strlen(hostName);
		sscanf(cPtr, "%d", &portN);
	}
	printf("* Factor : Port Number: %d\n", portN);

	fgets(line, 50, fp);
	fgets(line, 50, fp);
	sscanf(line, "%d %d", &maxN, &minN);
	printf("* Factor : Max and Min Limit: %d, %d\n", maxN, minN);

	sockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFd < 0) 
		perror("* Factor : Error opening Socket");

	memset(&servAddr, sizeof(servAddr), 0);

        servAddr.sin_family = AF_INET;
        servAddr.sin_addr.s_addr = INADDR_ANY;
        servAddr.sin_port = htons(portN);

        if (bind(sockFd, (const struct sockaddr*) &servAddr, sizeof(servAddr)) < 0)
        {
                perror("* Factor : Error Binding the Socket");
        }

        listen(sockFd, 5);
        clientLen = sizeof(clientAddr);
        parentFd = accept(sockFd, (struct sockaddr*) &clientAddr, (socklen_t *) &clientLen);
        if (-1 == parentFd)
        {
                perror("* Factor : Error Accepting the Socket 1");
        }

	buffer = 200;
	if( -1 == write(parentFd, &buffer, sizeof(buffer)))
	{
		perror("* Factor : Error writing to Parent");
		exit(-1);
	}

        ackFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ackFd < 0) 
		perror("* Factor : Error opening Socket");

        server = gethostbyname(hostName);
        if (NULL == server)
        {
                perror("* Factor : Error: No Such Host");
                exit(0);
        }

        bzero((char *) &servAddr, sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        bcopy((char *)server->h_addr, (char *)&servAddr.sin_addr.s_addr, server->h_length);
        servAddr.sin_port = htons(parPortN);

	fp = NULL;
	#if 1
	while(status)
	{
		/* Read from FAC Queue */
		if( -1 == read(parentFd, &num1, sizeof(int)))
		{
			perror("* Factor : Error reading from Parent");
			exit(-1);
		}
		if( -1 == read(parentFd, &num2, sizeof(int)))
		{
			perror("* Factor : Error reading from Parent");
			exit(-1);
		}

		/* If "fac 0 0" then exit, No reply to this message */
		if((0 == num1) && (0 == num2)) 
		{
			status = 0;
			printf("* ------------------\n");
			printf("* Factorial Exiting\n");
			printf("* ------------------\n");
			exit(0);
		}
		else
		{
			/* Calls facmod() function to calculate Factorial Mod */
			printf("* Factor : %d! Mod %d = %d\n", num1, num2, facmod(num1, num2));
		}
	
		compN++;
                //for(i = 0; i < 1000000; i++) rand();
                if(((recvN - compN) <= minN) && 1 == prntStop)
                {
                        /* Write Ack once addition is done */
                        printf("* Factor : Parent Can Send Data Now [%d - %d = %d]\n", recvN, compN, recvN-compN);
                        if( -1 == sendto(ackFd, &start, sizeof(int), 0, (struct sockaddr *)&servAddr, sizeof(servAddr)))
			{
				perror("* Factor : Error writing to Parent");
				exit(-1);
			}
			prntStop = 0;
                }
	}
	#endif

	printf("* Factorial Exiting\n");

	return 0;
}

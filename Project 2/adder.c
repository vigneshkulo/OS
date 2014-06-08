/*
 *	Submitted By	: Vignesh Kulothungan
 *	Subject		: Operating Systems - Project 2
 */

/* Header Files*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <signal.h>

int recvN = 0;
int compN = 0;
int maxN = 0;
int minN = 0;
int prntStop = 0;

void usr1Handler(int sigNo)
{
	recvN++;
//	printf("* Adder  : Received SIGUSR1 Signal [%d - %d = %d]\n", recvN, compN, recvN-compN);
	if((recvN - compN) >= maxN && 0 == prntStop) 
	{
//		printf("* Adder  : Stopping Parent: Limit Reached\n");
		kill(getppid(), SIGUSR1);
		prntStop = 1;
	}
}

/* Main Function */
int main(int argc, char *argv[])
{
	signal(SIGUSR1, usr1Handler);
	printf("* Adder  : SIGUSR1 Signal Registered [%d]\n", getpid());

	int clientLen = 0;
	int parentFd = 0;
	int ackFd = 0;
	int i = 0;
	int num = 0;
	int num1 = 0;
	int num2 = 0;
	int status = 1;
	int addCount = 0;
	int errorN = -1;
	long long int sum = 0;
	FILE *fp = NULL;
	char fileName[10];
	memset(fileName, 0, sizeof(fileName));

	int sockFd;
	int start = 1;
	struct hostent *server;
	struct sockaddr_in servAddr, clientAddr;

	int buffer;

	int portN = 0;
	int parPortN = 0;
	char line[50];
	char chkStr[5];
	char hostName[20];
	char *cPtr = NULL;

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
	printf("* Adder  : Parent Port Number: %d\n", parPortN);

	fgets(line, 50, fp);
	sscanf(line, "%s", chkStr);
	chkStr[5] = '\0';
	if(!strcmp(chkStr, "adder"))
	{
		cPtr = line;
		cPtr += 6;
		sscanf(cPtr, "%s", hostName);
		cPtr += strlen(hostName);
		sscanf(cPtr, "%d", &portN);
	}
	printf("* Adder  : Port Number: %d\n", portN);

	fgets(line, 50, fp);
	fgets(line, 50, fp);
	sscanf(line, "%d %d", &maxN, &minN);
	printf("* Adder  : Max and Min Limit: %d, %d\n", maxN, minN);

	sockFd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockFd < 0) 
		perror("* Adder  : Error opening Socket");

	memset(&servAddr, sizeof(servAddr), 0);

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = INADDR_ANY;
	servAddr.sin_port = htons(portN);

	if (bind(sockFd, (const struct sockaddr*) &servAddr, sizeof(servAddr)) < 0)
	{
		perror("* Adder  : Error Binding the Socket");
	}

	listen(sockFd, 5);
	clientLen = sizeof(clientAddr);
	parentFd = accept(sockFd, (struct sockaddr*) &clientAddr, (socklen_t *) &clientLen);
	if (-1 == parentFd)
	{
		perror("* Adder  : Error Accepting the Socket 1");
	}

	buffer = 100;
	if( -1 == write(parentFd, &buffer, sizeof(int)))
	{
		perror("* Adder  : Error writing to Parent");
		exit(-1);
	}

	ackFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ackFd < 0) 
		perror("* Adder  : Error opening Socket");

	server = gethostbyname(hostName);
	if (NULL == server)
	{
		perror("* Adder  : Error: No Such Host");
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
		/* Read from ADD Socket */
		if( -1 == read(parentFd, fileName, sizeof(fileName)))
		{
			perror("* Adder  : Error reading from Parent");
			exit(-1);
		}
		if( -1 == read(parentFd, &num2, sizeof(int)))
		{
			perror("* Adder  : Error reading from Parent");
			exit(-1);
		}
		
//		printf("* Adder  : %s, %d\n", fileName, num2);

		/* If "stopstop" then exit, No reply to this Message */
		if(!strcmp(fileName, "stopstop")) 
		{
			printf("* --------------\n");
			printf("* Adder Exiting\n");
			printf("* --------------\n");
			exit(0);
		}
		if(NULL == fp)	fp = fopen(fileName, "r+");

		/* If "error" in opening file, sent ack with MSB bit set to 1 */
		if(NULL == fp) 
		{
			printf("* Adder  : Unable to Open %s\n", fileName);
			continue;
		}
		fgets(line, 11, fp);
		sscanf(line, "%d", &num1);

		/* Calculate Sum */
		while(num1)
		{
			fgets(line, 15, fp);
			sscanf(line, "%d", &num);
			sum += num;
			num1--;
		}
		printf("* Adder  : %lli Mod %d = %lli\n", sum, num2, sum%num2);
		addCount++;


		#ifdef ACK_DEBUG
		printf("* Adder  [%2d]: Ack Sent for Msg: %d\n", addw, addCount);
		#endif

		sum = 0;
		num = 0;
		fclose(fp);
		fp = NULL;

		compN++;
	//	for(i = 0; i < 1000000; i++) rand();
		if(((recvN - compN) <= minN) && 1 == prntStop)
		{
			/* Write Ack once addition is done */
			printf("* Adder  : Parent Can Send Data Now [%d - %d = %d]\n", recvN, compN, recvN-compN);
			if( -1 == sendto(ackFd, &start, sizeof(int), 0, (struct sockaddr *)&servAddr, sizeof(servAddr)))
			{
				perror("* Adder  : Error writing to Parent");
				exit(-1);
			}
			prntStop = 0;
		}
	}
	#endif
	printf("* Adder Exiting\n");
	return 0;
}

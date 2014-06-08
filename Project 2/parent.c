/*
 *	Submitted By	: Vignesh Kulothungan
 *	Subject		: Operating Systems - Project 2 (Phase 2)
 */

/* Header Files*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

int stopFlag = 0;
int intrCount = 0;
void intrHandler(int sigNo)
{
	stopFlag = 1;
	intrCount++;
	printf("* Parent : Received STOP from Child\n");
}
/* Main Function */
int main(int argc, char *argv[])
{
	int i ,rn; 
	int delayD = 0; 
	int addPid = 0; 
	int factPid = 0;

	factPid = fork();
	if (factPid > 0)
	{ 
		addPid = fork();
		if(addPid > 0)
		{/* Parent */

			signal(SIGUSR1, intrHandler);
			int fd1, fd2;
			int ackFd;
			int portN[3];
			int sockFd[2];
			int addFd = 0;
			int addSent = 0;
			int factFd = 0;
			int facSent = 0;
			int clientLen = 0;

			FILE *fp = NULL;
			char *cPtr = NULL;

			char line[50];
			char chkStr[5];
			char hostName[20];
			int buffer, buffer1;
			struct hostent *server;
			struct sockaddr_in servAddr, clientAddr;

			fp = fopen("config.dat", "r+");
			if(NULL == fp)	exit(-1);
		
			fgets(line, 50, fp);

			sscanf(line, "%s", chkStr);
			chkStr[5] = '\0';

			if(!strcmp(chkStr, "mainp"))
			{
				cPtr = line;
				cPtr += 6;
				sscanf(cPtr, "%s", hostName);
				cPtr += strlen(hostName);
				sscanf(cPtr, "%d", &portN[0]);
			}
			printf("* Parent : Host Name  : %s\n", hostName);
			printf("* Parent : Port Number: %d\n", portN[0]);

			fgets(line, 50, fp);
			sscanf(line, "%s", chkStr);
			chkStr[5] = '\0';

			if(!strcmp(chkStr, "adder"))
			{
				cPtr = line;
				cPtr += 6;
				sscanf(cPtr, "%*s %d", &portN[1]);
			}
			printf("* Parent : Adder Port Number: %d\n", portN[1]);

			fgets(line, 50, fp);
			sscanf(line, "%s", chkStr);
			chkStr[5] = '\0';

			if(!strcmp(chkStr, "facto"))
			{
				cPtr = line;
				cPtr += 6;
				sscanf(cPtr, "%*s %d", &portN[2]);
			}
			printf("* Parent : Factorial Port Number: %d\n", portN[2]);

			fgets(line, 50, fp);
			sscanf(line, "%*d %*d %d", &delayD);
			printf("* Parent : Delay %d\n", delayD);

			server = gethostbyname(hostName);
			if (NULL == server)
			{
				perror("* Parent : Error: No Such Host");
				exit(0);
			}
	
			ackFd =socket(AF_INET,SOCK_DGRAM, 0);
                        if (-1 == ackFd)
                        {
                                perror("* Parent : Error Opening the Socket");
				exit(0);
                        }

			bzero(&servAddr,sizeof(servAddr));
			servAddr.sin_family = AF_INET;
			servAddr.sin_addr.s_addr=INADDR_ANY;
			servAddr.sin_port=htons(portN[0]);

			if( -1 == bind(ackFd, (struct sockaddr *)&servAddr,sizeof(servAddr)))
                        {
                                perror("* Parent : Error Binding the Socket");
				exit(0);
                        }

			sleep(1);

			for(i = 0; i < 2; i++)
			{
				sockFd[i] = socket(AF_INET, SOCK_STREAM, 0);
				if (-1 == sockFd[i]) 
				{
					perror("* Parent : Error Opening the Socket");
					exit(0);
				}

				memset(&servAddr, sizeof(servAddr), 0);

				servAddr.sin_family = AF_INET;
				bcopy((char *)server->h_addr, (char *)&servAddr.sin_addr.s_addr, server->h_length);
				servAddr.sin_port = htons(portN[i+1]);

				if (-1 == connect(sockFd[i],(struct sockaddr*) &servAddr,sizeof(servAddr)))
				{
					perror("* Parent : Error: Connecting");
					exit(-1);
				}
			}
		
			addFd = sockFd[0];
			factFd = sockFd[1];
			printf("* Parent : Add: %d, Fact: %d\n", addFd, factFd);

			if( -1 == read(addFd, &buffer, sizeof(int)))
			{
				perror("* Parent : Error Msg from Adder");
				exit(-1);
			}
                        printf("* Parent : %d From %d\n", buffer, addFd);

			if( -1 == read(factFd, &buffer, sizeof(int)))
			{
				perror("* Parent : Error Msg from Factorial");
				exit(-1);
			}
                        printf("* Parent : %d From %d\n", buffer, factFd);
			#if 1
			int ret = 0;
			int start = 0;
			int addN = 0;
			int facN = 0;
			int addRecv = 0;
			int facRecv = 0;
			int num1 = 0;
			int num2 = 0;
			char oper[4];
			int jobStat = 0;
			char fileName[10];
			memset(fileName, 0, sizeof(fileName));
			int status = 1;
			int lineLen = 0;
			ssize_t size = 30;
			char *linePtr = NULL;
			char *dataPtr = NULL;
			
			fp = fopen("instruction.dat", "r+");
			if(NULL == fp)	exit(-1);
			
			printf("---------------------------------------------\n");
			while(status)
			{
				/* Reads a line till \n is reached */
				lineLen = getline(&linePtr, &size, fp);
				if(0 >= lineLen) status = 0;
				else
				{
					linePtr[lineLen-1] = '\0';
					sscanf(linePtr, "%4s", oper);
					oper[3] = '\0';

					dataPtr = strchr(linePtr, ' ');
					dataPtr++;

					/* Enters if first 3 bytes are 'fac' */
					if(!strcmp(oper, "fac"))
					{
						/* Extract Value and Field */
						sscanf(dataPtr, "%d %d", &num1, &num2);
						if(1 == stopFlag)
						{
							while(intrCount)
							{
								printf("-----------------------------\n");
								if( -1 == read (ackFd, &start, sizeof(start)))
								{
									perror("* Parent : Error Ack from Factorial");
									exit(-1);
								}
								if(1 == start) stopFlag = 0;
								printf("* Parent : Restarting to Send Data: %d\n", start);
								start = 0;
								printf("-----------------------------\n");
								intrCount--;
							}
						}
						facSent++;
						printf("* Parent : [%2d] Sending %d and %d\n", facSent, num1, num2);
						if( -1 == write (factFd, &num1, sizeof(int)))
						{
							perror("* Parent : Error Writing to Factorial");
							exit(-1);
						}
						if( -1 == write (factFd, &num2, sizeof(int)))
						{
							perror("* Parent : Error Writing to Factorial");
							exit(-1);
						}
						kill(factPid, SIGUSR1);
						for(i = 0; i < delayD; i++) rn =rand();
					}
					/* Enters if first 3 bytes are 'add' */
					else if(!strcmp(oper, "add"))
					{
						/* Extract Filename and Field value */
						sscanf(dataPtr, "%8s %d", fileName, &num2);

						if(1 == stopFlag)
						{
							while(intrCount)
							{
								printf("-----------------------------\n");
								if( -1 == read (ackFd, &start, sizeof(start)))
								{
									perror("* Parent : Error Ack from Adder");
									exit(-1);
								}
								if(1 == start) stopFlag = 0;
								printf("* Parent : Restarting to Send Data: %d\n", start);
								start = 0;
								printf("-----------------------------\n");
								intrCount--;
							}
						}
						addSent++;
						printf("* Parent : [%2d] Sending %s and %d\n", addSent, fileName, num2);
						if( -1 == write (addFd, fileName, sizeof(fileName)))
						{
							perror("* Parent : Error Writing to Adder");
							exit(-1);
						}
						if( -1 == write (addFd, &num2, sizeof(int)))
						{
							perror("* Parent : Error Writing to Adder");
							exit(-1);
						}
						kill(addPid, SIGUSR1);
						for(i = 0; i < delayD; i++) rn =rand();
					}
					else
					{
						printf("* Invalid Operation\n");
					}
				}
			}

			if(NULL != fp) fclose(fp);
			#endif

			/* Parent waiting for Adder to exit, if "add stopstop 0" is not sent to Adder, Parent will get blocked in this step */
			printf("* Parent : Waiting for Adder to Exit\n");
			waitpid(addPid, NULL, 0);
			printf("* Parent : Adder Exited\n");
			printf("* ---------------------------------------------------------------\n");

			/* Parent waiting for Factorial to exit, if "fac 0 0" is not sent to Factorial, Parent will get blocked in this step */
			printf("* Parent : Waiting for Factorial to Exit\n");
			waitpid(factPid, NULL, 0);
			printf("* Parent : Factorial Exited\n");
			printf("* ---------------------------------------------------------------\n");


			printf("* ------------------\n");
			printf("* Parent Exiting\n");
			printf("* ------------------\n");

			/* Parent Exits */
		}
		else
		{ /* Adder */
			execv("adder", NULL);
			perror("* Adder: ");
		}
	}
	else
	{ /* Factorial */
		execv("factorial", NULL);
		perror("* Factorial: ");
	}
	return 0;
}

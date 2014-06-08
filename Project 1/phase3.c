/*
 *	Submitted By	: Vignesh Kulothungan
 *	Subject		: Operating Systems - Project 1
 */

/* Header Files*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

/* Required by fmod() */
#define _GNU_SOURCE

/* Uncomment following line to Print Acknowledgements */
//#define DEBUG
//#define ACK_DEBUG

/* Function Calculating Factorial and Factorial % Field */
double facmod(int n, int m)
{
	int i = 0;
	double fact = 1;
	double field = m;
	for(i = n; i > 0; i--)
	{
		fact*=i;
	}
	return fmod(fact,field);
}

/* Main Function */
int main(int argc, char *argv[])
{
	int limitN;

	/* Error Check for Command Line Input */
	printf("* --------------------------------------\n");
	if(NULL == argv[1]) 
	{
		printf("* Please Enter Queue Limit\n");
		scanf("%d", &limitN);
		printf("* --------------------------------------\n");
	}
	else
	{
		limitN = atoi(argv[1]);
		printf("* Queue Limit: %d\n", limitN);
	}

	if(limitN <= 0)
	{
		printf("* Negative or Zero Queue Limit Invalid. Re-enter Queue Limit\n");
		scanf("%d", &limitN);
		printf("* --------------------------------------\n");
	}

	int addPid = 0; 
	int factPid = 0;
	int fds1[2], fds2[2], fds3[2], fds4[2];
	int factr, factw, parentFactr, parentFactw;
	int addr, addw, parentAddr, parentAddw;

	pipe (fds1);
	pipe (fds2);

	pipe (fds3);
	pipe (fds4);

	factr = fds1[0];
	factw = fds2[1];
	parentFactr = fds2[0];
	parentFactw = fds1[1];

	addr = fds3[0];
	addw = fds4[1];
	parentAddr = fds4[0];
	parentAddw = fds3[1];
	
	#ifdef DEBUG
	printf("* --------------------------------------*\n");
	printf("* Factorial Read : %d\n", factr);
	printf("* Parent Fac Writ: %d\n", parentFactw);
	printf("* Parent Fac Read: %d\n", parentFactr);
	printf("* Factorial Write: %d\n", factw);
	printf("* Adder Read     : %d\n", addr);
	printf("* Parent Add Writ: %d\n", parentAddw);
	printf("* Parent Add Read: %d\n", parentAddr);
	printf("* Adder Write    : %d\n", addw);
	printf("* --------------------------------------*\n");
	#endif

	factPid = fork();
	if (factPid > 0)
	{ 
		addPid = fork();
		if(addPid > 0)
		{/* Parent */
			int ret = 0;
			int addN = 0;
			int facN = 0;
			int addSent = 0;
			int facSent = 0;
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
			FILE *fp = NULL;
			ssize_t size = 30;
			char *linePtr = NULL;
			char *dataPtr = NULL;
			

			close (factr); close (factw);
			close (addr); close (addw);

			fp = fopen("instruction.dat", "r+");
			if(NULL == fp)	exit(-1);
			
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
						write (parentFactw, &num1, sizeof(int));
						write (parentFactw, &num2, sizeof(int));

						facN++;			// Count to keep track of Pending instructions in FAC queue
						facSent++;		// Count to keep track of all instructions sent to Factorial.

						#ifdef ACK_DEBUG
						printf("* Parent [%2d]: Sent \"fac %d %d\" to Factorial: %d\n", parentFactw, num1, num2, facSent);
						#endif

						if(facN >= limitN)
						{/* Pending Instructions limit reached in FAC queue */
							#ifdef ACK_DEBUG
							printf("* Parent [%2d]: Waiting for Acknowledgement from Factorial\n", parentFactr);
							#endif

							/* Read Acknowledgement from Factorial */
							read(parentFactr, &jobStat, sizeof(int));

							/* If error occurs in Factorial, the MSB bit is set to 1, other bits carry the Message count */
							if((jobStat & 0x80000000) == 0x80000000) 
							{
								jobStat &= 0x7fffffff;
								#ifdef ACK_DEBUG
								printf("* Parent [%2d]: Error Ack for Msg: %d \n", parentFactr, jobStat);
								printf("* ---------------------------------------------------------------\n");
								#endif
							}
							/* If no error in Factorial, jobStat carries the Message count */
							else
							{
								#ifdef ACK_DEBUG
								printf("* Parent [%2d]: Received Acknowledgement from Factorial: %d \n", parentFactr, jobStat);
								printf("* ---------------------------------------------------------------\n");
								#endif
							}
							facRecv++;		// Count to keep track of Ack received from Factorial
							facN--;			
							jobStat = 0;		// jobStat reset to 0.
						}
					}
					/* Enters if first 3 bytes are 'add' */
					else if(!strcmp(oper, "add"))
					{
						/* Extract Filename and Field value */
						sscanf(dataPtr, "%8s %d", fileName, &num2);

						write (parentAddw, fileName, sizeof(fileName));
						write (parentAddw, &num2, sizeof(int));

						addN++;				// Count to keep track of Pending instructions in ADD queue
						addSent++;			// Count to keep track of all instructions sent to Adder.
						#ifdef ACK_DEBUG
						printf("* Parent [%2d]: Sent \"add %s %d\" to Adder: %d\n", parentAddw, fileName, num2, addSent);
						#endif

						if(addN >= limitN)
						{/* Pending Instructions limit reached in ADD queue */
							#ifdef ACK_DEBUG
							printf("* Parent [%2d]: Waiting for Acknowledgement from Adder\n", parentAddr);
							#endif

							read(parentAddr, &jobStat, sizeof(int));

							/* If error occurs in Adder, the MSB bit is set to 1, other bits carry the Message count */
							if((jobStat & 0x80000000) == 0x80000000) 
							{
								jobStat &= 0x7fffffff;
								#ifdef ACK_DEBUG
								printf("* Parent [%2d]: Error Ack for Msg: %d \n", parentAddr, jobStat);
								printf("* ---------------------------------------------------------------\n");
								#endif
							}
							/* If no error in Adder, jobStat carries the Message count */
							else
							{
								#ifdef ACK_DEBUG
								printf("* Parent [%2d]: Received Acknowledgement from Adder: %d \n", parentAddr, jobStat);
								printf("* ---------------------------------------------------------------\n");
								#endif
							}
							addRecv++;		// Count to keep track of Ack received from Adder.
							addN--;
							jobStat = 0;
						}
					}
					else
					{
						printf("* Invalid Operation\n");
					}
				}
			}

			#ifdef DEBUG
			printf("* ---------------------------------------------------------------\n");
			printf("* Parent : Reached End of File: %d, %d, %d, %d\n", facSent, facRecv, addSent, addRecv);
			printf("* ---------------------------------------------------------------\n");
			#endif
			
			/* 
			 * Check to see if all acknowledgements are received from both Factorial and Adder. 
			 * If limitN is 1, acknowledgment will be received after every read and send cycle, so this part is neglected. 
			 * If limitN > 1, then this part is executed.
			 */
			if(1 != limitN)
			{
				/* Ack will not be sent for "fac 0 0", so check to see if ((Sent -1) == Received)*/
				while((facSent-1) != facRecv)
				{
					read(parentFactr, &jobStat, sizeof(int));
					if((jobStat & 0x80000000) == 0x80000000) 
					{	
						jobStat &= 0x7fffffff;
						#ifdef ACK_DEBUG
						printf("* Parent [%2d]: Error Ack for Msg: %d \n", parentFactr, jobStat);
						printf("* ---------------------------------------------------------------\n");
						#endif
					}
					else
					{
						#ifdef ACK_DEBUG
						printf("* Parent [%2d]: Received Acknowledgement from Factorial: %d \n", parentFactr, jobStat);
						printf("* ---------------------------------------------------------------\n");
						#endif
					}
					facRecv++;
					jobStat = 0;
				}

				/* Ack will not be sent for "add stopstop 0", so check to see if ((Sent -1) == Received)*/
				while((addSent-1) != addRecv)
				{
					read(parentAddr, &jobStat, sizeof(int));
					if((jobStat & 0x80000000) == 0x80000000) 
					{
						jobStat &= 0x7fffffff;
						#ifdef ACK_DEBUG
						printf("* Parent [%2d]: Error Acknowledgement for Msg: %d \n", parentAddr, jobStat);
						printf("* ---------------------------------------------------------------\n");
						#endif
					}
					else
					{	
						#ifdef ACK_DEBUG
						printf("* Parent [%2d]: Received Acknowledgement from Adder: %d \n", parentAddr, jobStat);
						printf("* ---------------------------------------------------------------\n");
						#endif
					}
					addRecv++;
					jobStat = 0;
				}
			}
			#ifdef DEBUG
			printf("* ---------------------------------------------------------------\n");
			printf("* Parent : Before: %d, %d, %d, %d\n", facSent, facRecv, addSent, addRecv);
			printf("* ---------------------------------------------------------------\n");
			#endif

			/* Freeing memory allocated by 'getsline' */
			if(linePtr) free(linePtr);

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
			close (parentAddr); close (parentAddw);
			close (parentFactr); close (parentFactw);
			if(NULL != fp) fclose(fp);
			/* Parent Exits */
		}
		else
		{ /* Adder */
			int num = 0;
			double num1 = 0;
			int num2 = 0;
			int status = 1;
			int addCount = 0;
			int errorN = -1;
			double sum = 0;
			double field = 0;
			FILE *fp = NULL;
			char fileName[10];
			memset(fileName, 0, sizeof(fileName));
			char line[11];

			close (factr); close (factw);
			close (parentAddr); close (parentAddw);
			close (parentFactr); close (parentFactw);

			while(status)
			{
				/* Read from ADD Queue */
				read(addr, fileName, sizeof(fileName));
				read(addr, &num2, sizeof(int));
				
				/* If "stopstop" then exit, No reply to this Message */
				if(!strcmp(fileName, "stopstop")) 
				{
					printf("* --------------\n");
					printf("* Adder Exiting\n");
					printf("* --------------\n");
					close (addr); close (addw);
					exit(0);
				}
				if(NULL == fp)	fp = fopen(fileName, "r+");

				/* If "error" in opening file, sent ack with MSB bit set to 1 */
				if(NULL == fp) 
				{
					printf("* Adder  [%2d]: Unable to Open %s\n", addw, fileName);
					addCount++;
					errorN = addCount;
					errorN |= 0x80000000;
					write(addw, &errorN, sizeof(int));
					continue;
				}
				fgets(line, 11, fp);
				sscanf(line, "%lf", &num1);

				/* Calculate Sum */
				while(num1)
				{
					fgets(line, 11, fp);
					sscanf(line, "%d", &num);
					sum += num;
					num1--;
				}
				field = num2;
				printf("* Adder  : %.0lf Mod %d = %.0f\n", sum, num2, fmod(sum, field));
				addCount++;

				/* Write Ack once addition is done */
				write(addw, &addCount, sizeof(int));

				#ifdef ACK_DEBUG
				printf("* Adder  [%2d]: Ack Sent for Msg: %d\n", addw, addCount);
				#endif

				sum = 0;
				num = 0;
				fclose(fp);
				fp = NULL;
			}
			printf("* Adder Exiting\n");
			close (addr); close (addw);
		}
	}
	else
	{ /* Factorial */
		int num1 = 0;
		int num2 = 0;
		int factCount = 0;
		int status = 1;

		close (addr); close (addw);
		close (parentFactr); close (parentFactw);
		close (parentAddr); close (parentAddw);

		while(status)
		{
			/* Read from FAC Queue */
			read(factr, &num1, sizeof(int));
			read(factr, &num2, sizeof(int));

			/* If "fac 0 0" then exit, No reply to this message */
			if((0 == num1) && (0 == num2)) 
			{
				status = 0;
				printf("* ------------------\n");
				printf("* Factorial Exiting\n");
				printf("* ------------------\n");
				close (factr); close (factw);
				exit(0);
			}
			else
			{
				/* Calls facmod() function to calculate Factorial Mod */
				printf("* Factor : %d! Mod %d = %.0lf\n", num1, num2, facmod(num1, num2));
			}
			//sleep(5);
			factCount++;
		
			/* Write Ack once FactorialMod is done */
			write(factw, &factCount, sizeof(int));
			#ifdef ACK_DEBUG
			printf("* Factor [%2d]: Ack Sent for Msg: %d\n", factw, factCount);
			#endif
		}

		printf("* Factorial Exiting\n");
		close (factr); close (factw);
	}
	return 0;
}

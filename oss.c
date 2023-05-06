//Katelyn Bowers
//OSS - Project 6
//May 9, 2023
//oss.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <sys/msg.h>


#define PERMS 0644

//Message struct
struct my_msgbuf {
	long mtype;
	int pid;
	int request;
	int offset;
	int choice;
	bool faulted;
} my_msgbuf;


//Initializing shared memory for seconds
#define sec_key 25217904          


//Initializing shared memory for nano seconds
#define nano_key 25218510

#define max_processes 18


FILE *logFile;

struct pageTable {
	int pageSize;   //1K
	int frameNumber;
	int validBit;
	int maxSize;
	int pages[32];
};


//Process table blocks
struct PCB {
	int occupied;
	pid_t pid;
	int pageRequest;
	struct pageTable page;
	int memoryAddress;
	char FIFOHead;
};

struct frame {
	int dirtyBit;
	int occupied;
	int processPid;
	struct pageTable page;
	int referenceByte;
};


//Process table
struct PCB processTable[18] = {{0}};
struct frame frameTable[256];   //256K

struct PCB process;

void help();
bool isEmpty();
bool isFull();
void Enqueue(struct PCB process);
void Dequeue();
struct PCB Front();

static void myhandler(int s);
static int setupinterrupt();
static int setupitimer();


struct my_msgbuf message;
struct my_msgbuf received;
int msqid;
key_t key;



int main(int argc, char **argv) {
	int totalWorkers = 0, simulWorkers = 0, tempPid, i, c, billion = 1000000000, totalFrames = 256, pageSize = 1000, maxNewNano = 500000000, pageRequest = -1, nanoIncrement = 5500; 
	bool fileGiven = false, messageReceivedBool = false, doneRunning = false, doneCreating = false, verboseOn = false;
	char *userFile = NULL;
	struct PCB currentProcess;

	while((c = getopt(argc, argv, "hvf:")) != -1) {
		switch(c)
		{
			case 'h':
				help();
			case 'v':
				verboseOn = true;
				break;
			case 'f':
				userFile = optarg;
				fileGiven = true;
				break;
		}
	}


	printf("\nProgram is starting...");

	
	//Opening log file
	if(fileGiven) 
		logFile = fopen(userFile, "w");
	else
		logFile = fopen("logfile.txt", "w");


	//Setting up interrupt
	if(setupinterrupt() == -1) {
		perror("Failed to set up handler for SIGPROF");
		return 1;
	}

	if(setupitimer() == -1) {
		perror("Failed to set up the ITIMER_PROF interval timer");
		return 1;
	}

	signal(SIGINT, myhandler);

	for( ; ; )
	{
		int sec_id = shmget(sec_key, sizeof(int) * 10, IPC_CREAT | 0666);        //Allocating shared memory with key
		if(sec_id <= 0) {                                                       //Testing if shared memory allocation was successful or not
			fprintf(stderr, "Shared memory get failed\n");
			exit(1);
		}


		//Initializing shared memory for nano seconds
		int nano_id = shmget(nano_key, sizeof(int) * 10, IPC_CREAT | 0666);
		if(nano_id <= 0) {
			fprintf(stderr, "Shared memory for nanoseconds failed\n");
			exit(1);
		}


		const int *sec_ptr = (int *) shmat(sec_id, 0, 0);      //Pointer to shared memory address
		if(sec_ptr <= 0) {                               //Testing if pointer is actually working
			fprintf(stderr, "Shared memory attach failed\n");
			exit(1);
		}


		const int *nano_ptr = (int *) shmat(nano_id, 0, 0);
		if(nano_ptr <= 0) {
			fprintf(stderr, "Shared memory attachment for nanoseconds failed\n");
			exit(1);
		}



		//Setting seconds and nanoseconds to initial values
		int * seconds = (int *)(sec_ptr);
		*seconds = 0;

		int * nanoSeconds = (int *)(nano_ptr);
		*nanoSeconds = 0;

		//Message queue setup
		system("touch msgq.txt");
		if((key = ftok("msgq.txt", 1)) == -1) {
			perror("ftok");
			exit(1);
		}

		if((msqid = msgget(key, PERMS | IPC_CREAT)) == -1) {
			perror("msgget in parent");
			exit(1);
		}

		message.mtype = 1;

		//Getting current seconds and adding 3 to stop while loop after 3 real life seconds
		time_t startTime, endTime;
		startTime = time(NULL);
		endTime = startTime + 2;

		//Random new process time
		srand(getpid());
		int randomTime = (rand() % (maxNewNano - 1 + 1)) + 1;
		int chooseTimeNano = *seconds, chooseTimeSec = *seconds;
		if((*nanoSeconds + randomTime) < billion)
			chooseTimeNano += randomTime;
		else
		{
			chooseTimeNano = ((*seconds + randomTime) - billion);
			chooseTimeSec += 1;
		}




		while(totalWorkers <= 100 && (time(NULL) < endTime)) {	
			//If it's time to make another child, do so as long as there's less than 18 simultaneous already running
			if(*seconds > chooseTimeSec || (*seconds == chooseTimeSec && *nanoSeconds >= chooseTimeNano)) {
				if((simulWorkers < 18) && !doneCreating) {
					for(i = 0; i < 18; i++) {
						if(processTable[i].occupied == 0) {
							currentProcess = processTable[i];
							break;
						}
					}

					//Forking child
					tempPid = fork();
					
					//Setting new random time for next process creation
					randomTime = rand() % maxNewNano;
					chooseTimeNano = *seconds, chooseTimeSec = *seconds;
					if((*nanoSeconds + randomTime) < billion)
						chooseTimeNano += randomTime;
					else
					{
						chooseTimeNano = ((*seconds + randomTime) - billion);
						chooseTimeSec += 1;
					}

					//Filling out process table for child process
					currentProcess.occupied = 1;
					currentProcess.pid = tempPid;

					char* args[] = {"./worker", 0};

					//Execing child off
					if(tempPid < 0) { 
						perror("fork");
						printf("Terminating: fork failed");
						exit(1);
					} else if(tempPid == 0) {
						printf("execing a child: %d", getpid());
						execlp(args[0], args[0], args[1], NULL); 
						printf("Exec failed, terminating");
						exit(1);
					} 

					simulWorkers++;
					totalWorkers++;
				}
			}


			if(msgrcv(msqid, &received, sizeof(my_msgbuf), getpid(), IPC_NOWAIT) == -1) {
				if(errno == ENOMSG) {
					messageReceivedBool = false;
				} else {
					perror("\n\nFailed to receive message from child\n");
					exit(1);
				}
			} else {
				messageReceivedBool = true;
				for(i = 0; i < 18; i++) {
					pageRequest = message.request;
					if(processTable[i].pid == received.pid) {
						currentProcess = processTable[i];
						break;
					} 	
				}
			}





			//Increment clock 100 ns before message send






			if((*nanoSeconds + nanoIncrement) < billion)
				*nanoSeconds += nanoIncrement;
			else
			{
				*nanoSeconds = ((*nanoSeconds + nanoIncrement) - billion);
				*seconds += 1;
			}
		}




		//Deallocating shared memory
		shmdt(sec_ptr);
		shmctl(sec_id, IPC_RMID, NULL);

		shmdt(nano_ptr);
		shmctl(nano_id, IPC_RMID, NULL);

		//Closing log file
		fclose(logFile);

		//Closing message queue
		if(msgctl(msqid, IPC_RMID, NULL) == -1) {
			perror("msgctl");
			exit(1);
		}

		printf("\n\n\n\nProgram is done\n");
		return(0);
	}
}










void help() {
	printf("\nThis program takes in a logfile or uses a deault file to log all of the output created while running");
	printf("\nIn this program, the FIFO plage replacement algorithm is used to simulate processes who require and request more space");
	printf("\nWhen the program begins, it will start forking off processes, only allowing 18 to run simultaneously, and each process will continuously ask for more space");
	printf("\nThe program will terminate after either over 100 processes have been created or 2 real life seconds have passed.");
	printf("\n\nInput options:");
	printf("\n-h     output a short description of the project and how to run it");
	printf("\n-f     the name of the file for output to be logged to");
	printf("\n\nInput example");
	printf("\n./oss -f logfile.txt");
}



struct PCB queue[max_processes];

//Queue function pointers
int front =  -1; 
int rear = -1;


bool isEmpty() {
	return(front == -1 && rear == -1);
}

bool isFull() {
	if((rear + 1) + front == max_processes)
		return true;

	return false;
}



void Enqueue(struct PCB process) {
	if(isFull()) 
		return;
	if(isEmpty()) {
		front = rear = 0;
	} else {
		rear += 1;
		if(rear == max_processes)
			rear = rear % max_processes;
	}

	queue[rear] = process;	
}


void Dequeue() {
	if(isEmpty()) {
		printf("\n\nError: Ready queue is empty\n\n");
		return;
	} else if(front == rear) 
		rear = front = -1;
	else {
		front += 1;
		if(front == max_processes)
			front = front % max_processes;
	}
}


struct PCB Front() {
	if(front == -1) {
		printf("\n\nError: Cannot return front of an empty queue");
		exit(1);
	}
	return queue[front];
}




//Handler function for signal to stop program after 60 seconds or with Ctrl + C
static void myhandler(int s) {
	int i, pid;
	for(i = 0; i <= 19; i++) {
		pid = processTable[i].pid;
		kill(pid, SIGKILL);
	}

	int sec_id = shmget(sec_key, sizeof(int) * 10, IPC_CREAT | 0666);        //Allocating shared memory with key
	if(sec_id <= 0) {                                                       //Testing if shared memory allocation was successful or not
		fprintf(stderr, "Shared memory get failed\n");
		exit(1);
	}


	//Initializing shared memory for nano seconds
	int nano_id = shmget(nano_key, sizeof(int) * 10, IPC_CREAT | 0666);
	if(nano_id <= 0) {
		fprintf(stderr, "Shared memory for nanoseconds failed\n");
		exit(1);
	}


	const int *sec_ptr = (int *) shmat(sec_id, 0, 0);      //Pointer to shared memory address
	if(sec_ptr <= 0) {                               //Testing if pointer is actually working
		fprintf(stderr, "Shared memory attach failed\n");
		exit(1);
	}


	const int *nano_ptr = (int *) shmat(nano_id, 0, 0);
	if(nano_ptr <= 0) {
		fprintf(stderr, "Shared memory attachment for nanoseconds failed\n");
		exit(1);
	}


	shmdt(sec_ptr);
	shmctl(sec_id, IPC_RMID, NULL);
	shmdt(nano_ptr);
	shmctl(nano_id, IPC_RMID, NULL);
	fclose(logFile);

	if(msgctl(msqid, IPC_RMID, NULL) == -1) {
		perror("msgctl");
		exit(1);
	}
	exit(1);
}


//Interrupt and timer functions for signal
static int setupinterrupt(void) {
	struct sigaction act;
	act.sa_handler = myhandler;
	act.sa_flags = 0;
	return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}

static int setupitimer(void) {
	struct itimerval value;
	value.it_interval.tv_sec = 60;
	value.it_interval.tv_usec = 0;
	value.it_value = value.it_interval;
	return (setitimer(ITIMER_PROF, &value, NULL));
}

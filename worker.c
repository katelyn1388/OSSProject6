//Katelyn Bowers
//OSS- Project 6
//May 9, 2023
//worker.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <sys/msg.h>
#include <errno.h>
#include <string.h>



#define PERMS 0644
struct my_msgbuf {
	long mtype;
	int pid;
	int request;
	int offset;
	int choice;
	bool faulted;
} my_msgbuf;



int main(int argc, char** iterations) {
	struct my_msgbuf message;
	struct my_msgbuf received;
	int msqid;
	key_t key;
	message.mtype = getppid();
	received.mtype = 1;
	message.pid = getpid();


	//Making sure message queue works	
	if((key = ftok("msgq.txt", 1)) == -1) {
		perror("ftok");
		exit(1);
	}

	if((msqid = msgget(key, PERMS)) == -1) {
		perror("msgget failed in child");
		exit(1);
	}


	//Attaching to shared memory from here til line 81
	const int sec_key = 25217904;
	const int nano_key = 25218510;

	int sec_id = shmget(sec_key, sizeof(int) * 10, IPC_CREAT | 0666);
	if(sec_id <= 0) {
		fprintf(stderr, "Shared memory get for seconds in worker failed\n");
		exit(1);
	}

	int *sec_ptr = (int *) shmat(sec_id, 0, 0);
	if(sec_ptr <= 0) {
		fprintf(stderr, "Shared memory attachment for seconds in worker failed\n");
		exit(1);
	}

	int nano_id = shmget(nano_key, sizeof(int) * 10, IPC_CREAT | 0666);
	if(nano_id <= 0) {
		fprintf(stderr, "Shared memory get for nanoseconds in worker failed\n");
		exit(1);
	}


	int *nano_ptr = (int *) shmat(nano_id, 0, 0);
	if(nano_ptr <= 0) {
		fprintf(stderr, "Shared memory attachment for nanoseconds in worker failed\n");
		exit(1);
	}

	int * sharedSeconds = (int *)(sec_ptr);
	int * sharedNanoSeconds = (int *)(nano_ptr);


	printf("\nWorker started: %d\n", getpid());


	int memoryReferences, terminateCheck = 1000, terminateRandomNum, choiceNum;
	int memoryPerSec, pageFaults, memAccessSpeedSec, memAccessSpeedNano, nextSecond = *sharedSeconds + 1, startingSecond = *sharedSeconds, memoryAccesses;
	int beforeAccessSec, beforeAccessNano, afterAccessSec, afterAccessNano;
	double memAccessSpeed;
	bool terminate = false, messageReceived;
	char seconds[20];
	char nanoSeconds[20];



	while(!terminate) {
		messageReceived = false;
		message.request = (rand() % (31 - 0 + 1));
		message.offset = ((message.request * 1024) + (rand() % (1023 - 0 + 1)));
		choiceNum = (rand() % (100 - 0 + 1));
		if(choiceNum < 80) {
			message.choice = 1;   //Read
		} else {
			message.choice = 2;   //Write
		}

		if((memoryReferences % terminateCheck) == 0 && memoryReferences > 1) {
			//Decide to terminate or not
			terminateRandomNum = (rand() % (100 - 1 + 1) + 1);
			if(terminateRandomNum < 20) {
				terminate = true;
				message.choice = 3;
			}
			printf("\nProcess is choosing to terminate");
		}


		printf("\n%d sending message to parent requesting %d", getpid(), message.request);
		
		if(msgsnd(msqid, &message, sizeof(my_msgbuf) - sizeof(long), 0) == -1) {
			perror("\nmsgsend to parent failed");
			exit(1);
		}

		beforeAccessSec = *sharedSeconds;
		beforeAccessNano = *sharedNanoSeconds;

		while(!messageReceived) {
			if(msgrcv(msqid, &received, sizeof(my_msgbuf), getpid(), 0) == -1) {
				if(errno == ENOMSG) {
					messageReceived = false;
				} else {
					perror("\nmsgrcv from parent failed");
					exit(1);
				}
			} else
				messageReceived = true;

		}

		afterAccessSec = *sharedSeconds;
		afterAccessNano = *sharedNanoSeconds;
		memAccessSpeedSec += (afterAccessSec - beforeAccessSec);
		memAccessSpeedNano += (afterAccessNano - beforeAccessNano);
		sprintf(seconds, "%d.", memAccessSpeedSec);
		sprintf(nanoSeconds, "%d", memAccessSpeedNano);
		strcat(seconds, nanoSeconds);
		memAccessSpeed += atof(seconds);



		if(received.choice == 1 || received.choice == 2) {
			memoryReferences++;
			memoryAccesses++;
			if(received.faulted == true) 
				pageFaults++;
		}

		if(nextSecond <= *sharedSeconds) {
			nextSecond++;
			memoryPerSec += memoryAccesses;
			memoryAccesses = 0;
		}



	}





	printf("\nChild %d is terminating", getpid());

	printf("\n%d:  number of memory accesses per second: %f", getpid(), ((double)memoryPerSec / (double)(*sharedSeconds - startingSecond)));
	printf("\n%d:  number of page faults per memory access: %f", getpid(), ((double)pageFaults / (double)memoryReferences));
	printf("\n%d:  average memory access speed: %f", getpid(), (memAccessSpeed / (double)memoryReferences));   

	return 0;

}

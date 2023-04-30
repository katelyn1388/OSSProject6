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



#define PERMS 0644
struct my_msgbuf {
	long mtype;
	int pid;
	int request;
}my_msgbuf;



int main(int argc, char** iterations) {
	struct my_msgbuf message;
	struct my_msgbuf received;
	int msqid, i;
	key_t key;
	message.mtype = getppid();
	message.intData = getppid();
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

	int starterNano = *sharedNanoSeconds;
	int starterSec = *sharedSeconds + 1;


	printf("\nWorker started: %d\n", getpid());

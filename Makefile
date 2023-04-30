CC = gcc -g3
CFLAGS = -g3 -Wall
TARGET1 = oss
TARGET2 = worker

OBJS1 = oss.o
OBJS2 = worker.o 

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(OBJS1)
	$(CC) -o $(TARGET1) $(OBJS1)

$(TARGET2): $(OBJS2) 
	$(CC) -o $(TARGET2) $(OBJS2) 

exec_master.o:	oss.c 
	$(CC) $(CFLAGS) -c oss.c 

exec_worker.o:	worker.c
	$(CC) $(CFLAGS) -c worker.c 

clean:
	/bin/rm -f *.o $(TARGET1) $(TARGET2)
	/bin/rm -f *.txt

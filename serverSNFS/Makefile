FLAGS = -W -Wall -pedantic -g -pthread
CC = gcc

all: serverSNFS

serverSNFS: serverSNFS.o
	$(CC) $(FLAGS) -o serverSNFS serverSNFS.o

serverSNFS.o: serverSNFS.c
	$(CC) $(FLAGS) -c serverSNFS.c

clean:
	rm -rf *.o serverSNFS
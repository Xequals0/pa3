FLAGS = -W -Wall -g -pthread
CC = gcc

all: serverSNFS

serverSNFS: serverSNFS.o
	$(CC) $(FLAGS) -o serverSNFS serverSNFS.o

serverSNFS.o: serverSNFS.c
	$(CC) $(FLAGS) -c serverSNFS.c -D_FILE_OFFSET_BITS=64

clean:
	rm -rf *.o serverSNFS

FLAGS = -W -Wall -g `pkg-config fuse --cflags --libs`
CC = gcc

all: clientSNFS

clientSNFS: clientSNFS.o
	$(CC) $(FLAGS) -o clientSNFS clientSNFS.o

clientSNFS.o: clientSNFS.c
	$(CC) $(FLAGS) -c clientSNFS.c

clean:
	rm -rf *.o clientSNFS

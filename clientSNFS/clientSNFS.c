#define FUSE_USE_VERSION 29
#define _FILE_OFFSET_BITS 64

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

int getFlags(char *flags);
int openConnection();
int snfs_open(const char *path, struct fuse_file_info *fi);
int snfs_mkdir(const char *path, mode_t mode);
int snfs_getattr(const char *path, struct stat *stbuf);
int snfs_readdir(const char* path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
int snfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int snfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int snfs_truncate(const char *path, off_t size);

static struct fuse_operations snfs_oper = {
    .open = snfs_open,
    //.close = snfs_close
    //.create = snfs_create,
    .truncate = snfs_truncate,
    .getattr = snfs_getattr,
    .read = snfs_read,
    .write = snfs_write,
    /*.opendir = snfs_opendir,*/
    .readdir = snfs_readdir,/*
    .releasedir = snfs_releasedir,*/
    .mkdir = snfs_mkdir
};

//int clientSocket;
int main(int argc, char* argv[])
{
    int i;
    char* port;
    char* address;
    char* mount;
    
    for(i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-port") == 0)
        {
            i++;
            port = (char*)malloc(strlen(argv[i]) + 1);
            strcpy(port, argv[i]);
        }
        else if(strcmp(argv[i], "-address") == 0)
        {
            i++;
            address = (char*)malloc(strlen(argv[i]) + 1);
            strcpy(address, argv[i]);
            
        }
        else if(strcmp(argv[i], "-mount") == 0)
        {
            i++;
            mount = (char*)malloc(strlen(argv[i]) + 1);
            strcpy(mount, argv[i]);
        }
    }
    
    return fuse_main(argc, argv, &snfs_oper, NULL);
}

int snfs_open(const char *path, struct fuse_file_info *fi){
		
	int connection = openConnection();
	if(connection < 0){
		h_errno = HOST_NOT_FOUND;
		return -1;
	}

	int command = htonl(0);

	printf("Sending open command to the server\n");
	if(send(connection, &command, sizeof(command), 0) < 0)
		printf("Error sending open command to the server\n");
	sleep(1);
	
	//send path to the server
    int pathLen  = strlen(path) + 1;
    
    int numBytesSent = send(connection, path, pathLen, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    else
        printf("Success sending path(%s) to the server\n", path);
    
    sleep(1);

	//send flags to server
	int flagMessage = htonl(fi->flags);
	if(send(connection, &flagMessage, sizeof(flagMessage), 0) < 0)
		printf("Error sending flags to the server\n");
	else
		printf("Success sending flags(%d) to the server\n", fi->flags);
	
	printf("Waiting for response from server\n");
    
    //recv fd
	int output_fd;
	if(recv(connection, &output_fd, sizeof(output_fd), 0) == -1){
		perror("Error receiving fd from the server\n");
		return -1;		
	}

    fi->fh = output_fd;

	printf("fd recveived was %d\n", output_fd);

	//receive any errors
	int errorMessage;
	if(output_fd == -1){
		if(recv(connection, &errorMessage, sizeof(errorMessage), 0) == -1){
			perror("Could not recieve error message from the server\n");
			return -1;
		}
		errno = ntohl(errorMessage);
		perror("Error from the server\n");
	}
	printf("\n");
	close(connection);
    
	return 0;	
}
/*
int snfs_close(int fd){

	int connection = openConnection();
	if(connection < 0){
		h_errno = HOST_NOT_FOUND;
		return -1;
	}

	int nClose = htonl(3);
	if(send(connection, &nClose, sizeof(nClose), 0) == -1){
		perror("Could not send close command to server\n");
	}

	sleep(1);

	int msg = htonl(fd);
	if(send(connection, &msg, sizeof(msg), 0) == -1)
		perror("Fd could not be sent to the server\n");

	int retVal;
	if(recv(connection, &retVal, sizeof(retVal), 0) == -1)
		perror("Return value from the server was not received\n");

	int result = ntohl(retVal);
	
	if(result == -1){
		int error;
		if(recv(connection, &error, sizeof(error), 0) == -1)
			perror("Could not get error code from the server\n");

		errno = ntohl(error);
		perror("Error when trying to close fd\n");
	}
	printf("\n");
	close(connection);

	return result;
}*/

int snfs_truncate(const char *path, off_t size){
    int connection = openConnection();

    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(5);
    int pathLen = strlen(path) + 1;

    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending getattr command to the server\n");
    
    sleep(1);

    int numBytesSent = send(connection, path, pathLen, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    else
        printf("Success sending path(%s) to the server\n", path);

    //send size
    int sizeN = htonl(size);
    
    printf("Sending size to the server\n");
    if(send(connection, &sizeN, sizeof(sizeN), 0) < 0)
        printf("Error sending size to the server\n");
    
    sleep(1);

    //recv return value
    int retVal;
    if(recv(connection, &retVal, sizeof(retVal), 0) == -1)
        perror("Return value from the server was not received\n");
    
    int result = ntohl(retVal);
    
    //receive any errors
    int errorMessage;
    if(result == -1){
        if(recv(connection, &errorMessage, sizeof(errorMessage), 0) == -1){
            perror("Could not error message from the server\n");
            return -1;
        }
        errno = ntohl(errorMessage);
        perror("Error from the server\n");
        result = -errno;
    }

    return result;
}

int snfs_getattr(const char *path, struct stat *stbuf){
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(6);
    
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending getattr command to the server\n");
    
    sleep(1);
    
    //send path to the server
    int pathLen  = strlen(path) + 1;
    int pathLength = htonl(pathLen);
    
    int numBytesSent = send(connection, path, pathLength, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    else
        //printf("Success sending path(%s) to the server\n", path);
    
    sleep(1);
        
    //recv return value
    int retVal;
    if(recv(connection, &retVal, sizeof(retVal), 0) == -1)
        perror("Return value from the server was not received\n");
    
    int result = ntohl(retVal);
    
    //receive any errors
    int errorMessage;
    if(result == -1){
        if(recv(connection, &errorMessage, sizeof(errorMessage), 0) == -1){
            perror("Could not error message from the server\n");
            return -1;
        }
        errno = ntohl(errorMessage);
        perror("Error from the server\n");
        result = -errno;
    }
    else if(result == 0)
    {
        if(recv(connection, stbuf, sizeof(struct stat), 0) == -1){
            perror("Could not recieve stat struct from the server\n");
            return -1;
        }

    }
    close(connection);
    
    return result;
}

int getFlags(char *flags){
	if(strcmp(flags, "r") == 0)
		return 0;
	else if(strcmp(flags, "w") == 0)
		return 1;
	else if(strcmp(flags, "rw") == 0)
		return 2;
	else 
		return -1;
}

int snfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    (void*) path;
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(1);
    
    printf("Sending read command to the server\n");
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending read command to the server\n");
    
    sleep(1);
    
    //send fi
    printf("!-!-%d\n", fi->fh);
    int numBytesSent = send(connection, &(fi->fh), sizeof(uint64_t), 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    else
        printf("Success sending fi to the server\n");
    
    sleep(1);
    
    //send size    
    printf("Sending size to the server\n");
    if(send(connection, &size, sizeof(size_t), 0) < 0)
        printf("Error sending size to the server\n");
    
    sleep(1);
    
    //send offset    
    printf("Sending offset to the server\n");
    if(send(connection, &offset, sizeof(off_t), 0) < 0)
        printf("Error sending offset to the server\n");
    
    sleep(1);
    
    //recv return value
    int retVal;
    if(recv(connection, &retVal, sizeof(retVal), 0) == -1)
        perror("Return value from the server was not received\n");
    
    int result = ntohl(retVal);
    
    //receive any errors
    int errorMessage;
    if(result == -1){
        if(recv(connection, &errorMessage, sizeof(errorMessage), 0) == -1){
            perror("Could not recieve error message from the server\n");
            return -1;
        }
        errno = ntohl(errorMessage);
        perror("Error from the server\n");
    }
    else
    {
        if(recv(connection, buf, size, 0) == -1){
            perror("Could not recieve buf from the server\n");
            return -1;
        }
    }
    printf("\n");
    close(connection);
    
    return result;
}

int snfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    (void*) path;
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(2);
    
    printf("Sending write command to the server\n");
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending write command to the server\n");
    
    sleep(1);
    
    //send handle
    int numBytesSent = send(connection, &(fi->fh), sizeof(uint64_t), 0);
    if(numBytesSent < 0)
        printf("Error sending buf to the server\n");
    else
        printf("Success sending buf(%s) to the server\n", buf);
    
    sleep(1);
    
    //send size
    int sizeN = htonl(size);
    
    printf("Sending size to the server\n");
    if(send(connection, &sizeN, sizeof(sizeN), 0) < 0)
        printf("Error sending size to the server\n");
    
    sleep(1);

    //send buf
    numBytesSent = send(connection, buf, size, 0);
    if(numBytesSent < 0)
        printf("Error sending buf to the server\n");
    else
        printf("Success sending buf(%s) to the server\n", buf);
    
    sleep(1);  
    
    //send offset
    int offsetN = htonl(offset);
    
    printf("Sending offset to the server\n");
    if(send(connection, &offsetN, sizeof(offsetN), 0) < 0)
        printf("Error sending offset to the server\n");
    
    sleep(1);
    
    //recv return value
    int retVal;
    if(recv(connection, &retVal, sizeof(retVal), 0) == -1)
        perror("Return value from the server was not received\n");
    
    int result = ntohl(retVal);
    
    //receive any errors
    int errorMessage;
    if(result == -1){
        if(recv(connection, &errorMessage, sizeof(errorMessage), 0) == -1){
            perror("Could not recieve error message from the server\n");
            return -1;
        }
        errno = ntohl(errorMessage);
        perror("Error from the server\n");
    }
    printf("\n");
    close(connection);
     
    return result;
}
/*
int snfs_opendir(const char *path, struct fuse_file_info *fi){

}*/

int snfs_readdir(const char* path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
    (void) offset;
    (void*) fi;
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(8);
    
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending readdir command to the server\n");
    
    sleep(1);
    
    //send path to the server
    int pathLen  = strlen(path) + 1;
    int pathLength = htonl(pathLen);
    
    int numBytesSent = send(connection, path, pathLength, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    else
        printf("Success sending path(%s) to the server\n", path);
    
    sleep(1);

    int x;
    while(1)
    {
        recv(connection, &x, sizeof(int), 0);
        if(x == -1) break;

        struct stat st;
        memset(&st, 0, sizeof(st));
        char dname[256];
        recv(connection, dname, x, 0);
        recv(connection, &st, sizeof(struct stat), 0);
        if(filler(buf, dname, &st, 0)) break;
    }

    if(x != -1) recv(connection, &x, sizeof(int), 0);

    return 0;
}
/*
int releasedir(const char *path, struct fuse_file_info *fi){

}*/

int snfs_mkdir(const char *path, mode_t mode){
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(10);
    
    printf("Sending mkdir command to the server\n");
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending open command to the server\n");
    
    sleep(1);
    
    //send path to the server
    int pathLen  = strlen(path) + 1;
    int pathLength = htonl(pathLen);
    
    int numBytesSent = send(connection, path, pathLength, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    else
        printf("Success sending path(%s) to the server\n", path);
    
    sleep(1);
    
    //send mode to server    
    numBytesSent = send(connection, &mode, sizeof(mode), 0);
    if(numBytesSent < 0)
        printf("Error sending modeN to the server\n");
    else
    
    sleep(1);
    
    //recv return value
    int retVal;
    if(recv(connection, &retVal, sizeof(retVal), 0) == -1)
        perror("Return value from the server was not received\n");
    
    int result = ntohl(retVal);
    
    //receive any errors
    int errorMessage;
    if(result == -1){
        if(recv(connection, &errorMessage, sizeof(errorMessage), 0) == -1){
            perror("Could not recieve error message from the server\n");
            return -1;
        }
        errno = ntohl(errorMessage);
        perror("Error from the server\n");
    }
    printf("\n");
    close(connection);
    
    return result;
}

int openConnection(){
    int port = 13175;
    
    int clientSocket;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        printf("Failed to create the client socket. Error: %d\n",errno);
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    server = gethostbyname("ls.cs.rutgers.edu"); //Update this later
    if (server == 0) { //Not sure if I did this right?
        printf("Failed to resolve the host. Error: %d\n",errno);
        exit(1);
    }
    memcpy(&serv_addr.sin_addr, server->h_addr, server->h_length);
    serv_addr.sin_port=htons(port);
      
    if(connect(clientSocket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Failed to bind. Error: %d\n",errno);
        close(clientSocket);
        exit(1);
    }
	return clientSocket;
}
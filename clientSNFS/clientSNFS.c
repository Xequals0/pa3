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


int getFlags(char *flags);
int snfs_close(int fd);
int snfs_open(const char *pathname, int flags);
int openConnection();

//int clientSocket;
int main(int argc, const char* argv[])
{
	if(argc != 3){
		printf("Usage %s: <host> <port>\n", argv[0]);
		return 0;
	}

	//char* host = argv[1];
	//char *port = argv[2]; 
   
	while(1){
		printf("Enter one of the following:\n\t\"open <filename>\"\n\t\"close <file descriptor>\"\n\t\"quit\"\n");
		int i;
		char input[100];
		char commands[4][100]; // stores command and its arguements, ex: read, write, etc
		
		while(fgets(input, sizeof(input), stdin) == NULL){ ; }
		
		//remove newline character
		char*ch;
		if((ch=strchr(input, '\n')) != NULL)
			*ch = '\0';

		char *tok = input, *tmp = input;
		int first = 1;
		for(i = 0; i < 4; i++){
			if(tok == NULL) break;
			strsep(&tmp, " ");

			if(tok[0] == '\0' && first != 1){
				i--;
			}
			else
				strcpy(commands[i], tok);

			tok = tmp;

			if(first == 1) first = 0;
		}

		char *endptr = (char *)calloc(101, sizeof(char));
		if(strcmp("quit", commands[0]) == 0)
			break;
		else if(strcmp("open", commands[0]) == 0){
			char *path = commands[1];
			printf("flag is: %s\n", commands[2]);
			int flags = getFlags(commands[2]);
			if(snfs_open(path, flags) == -1){
				if(errno == 0)
					herror("Error opening file\n");
				else
					perror("Error opening file\n");
			}	
		}
		else if(strcmp("close", commands[0]) == 0){
			int fd = strtol(commands[1], &endptr, 10);
			if(snfs_close(fd) == -1){
				if(errno == 0)
					herror("Error opening file\n");
				else
					perror("Error opening file\n");
			}			
		}
	}


    return 0;
}

int snfs_open(const char *pathname, int flags){
		
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
	
	//send filename to the server
	int pathnameLen  = strlen(pathname);
	int numBytesSent = send(connection, pathname, pathnameLen, 0);
	if(numBytesSent < 0)
		printf("Error sending pathname to the server\n");
	else
		printf("Success sending pathname(%s) to the server\n", pathname);
	sleep(1);

	//send flags to server
	int flagMessage = htonl(flags);
	if(send(connection, &flagMessage, sizeof(flagMessage), 0) < 0)
		printf("Error sending flags to the server\n");
	else
		printf("Success sending flags(%d) to the server\n", flags);
	
	printf("Waiting for response from server\n");
	int output_fd;
	if(recv(connection, &output_fd, sizeof(output_fd), 0) == -1){
		perror("Error receiving fd from the server\n");
		return -1;		
	}

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
	return output_fd;	
}

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
}

int snfs_truncate(const char *path, uid_t uid, gid_t gid){

}

int snfs_getattr(const char *path, struct stat *stbuf){
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(6);
    
    printf("Sending open command to the server\n");
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending open command to the server\n");
    
    sleep(1);
    
    //send path to the server
    int pathLen  = strlen(path)
    int pathLength = htonl(pathLen);
    
    int numBytesSent = send(connection, path, pathLength, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    else
        printf("Success sending path(%s) to the server\n", path);
    
    sleep(1);
    
    //send stbuf to server
    
    
 
    
    
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

}

int snfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){

}

int snfs_opendir(const char *path, struct fuse_file_info *fi){

}

int snfs_readdir(const char* path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
    
}

int releasedir(const char *path, struct fuse_file_info *fi){

}

int snfs_mkdir(const char *path, mode_t mode){
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(10);
    
    printf("Sending open command to the server\n");
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending open command to the server\n");
    
    sleep(1);
    
    //send path to the server
    int pathLen  = strlen(path)
    int pathLength = htonl(pathLen);
    
    int numBytesSent = send(connection, path, pathLength, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    else
        printf("Success sending path(%s) to the server\n", path);
    
    sleep(1);
    
    //send mode to server
    int modeN = htonl(mode);
    
    int numBytesSent = send(connection, &modeN, sizeof(modeN), 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    else
        printf("Success sending path(%s) to the server\n", path);
    
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
    server = gethostbyname("localhost"); //Update this later
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

static struct fuse_operations snfs_oper = {
	.open = snfs_open,
	//.close = snfs_close
	/*.create = snfs_create,
	.truncate = snfs_truncate,
	.getattr = snfs_getattr,
	.read = snfs_read,
	.write = snfs_write,
	.opendir = snfs_opendir,
	.readdir = snfs_readdir,
	.releasedir = snfs_releasedir,
	.mkdir = snfs_mkdir */
};

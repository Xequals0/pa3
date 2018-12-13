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
int snfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int snfs_opendir(const char *path, struct fuse_file_info *fi);
int snfs_flush(const char *path, struct fuse_file_info *fi);
int snfs_release(const char *path, struct fuse_file_info *fi);
int snfs_releasedir(const char *path, struct fuse_file_info *fi);

int portNum;
char *address;


static struct fuse_operations snfs_oper = {
    .open = snfs_open,
    .flush = snfs_flush,
    .release = snfs_release,
    .create = snfs_create,
    .truncate = snfs_truncate,
    .getattr = snfs_getattr,
    .read = snfs_read,
    .write = snfs_write,
    .opendir = snfs_opendir,
    .readdir = snfs_readdir,
    .releasedir = snfs_releasedir,
    .mkdir = snfs_mkdir
};

int sendall(int socket, const char* buffer, int length)
{
   int n;
   const char *sendptr = buffer;
   while(length > 0)
   {
      n = send(socket, sendptr, length, 0);
      if(n <= 0) return -1;
      sendptr += n;
      length -= n;
   }

   return 0;
}

int recvall(int socket, char* buffer, int length)
{
   int n;
   int i = 0;
   while(length > 0)
   {
      n = recv(socket, buffer + i, length, 0);
      if(n <= 0) return -1;
      i += n;
      length -= n;
   }

   return 0;
} 

//int clientSocket;
int main(int argc, char* argv[])
{
    int argc2 = 2;
    char* argv2[2];
    argv2[0] = argv[0];

    int i;
    char* mount;
    
    for(i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-port") == 0)
        {
            i++;
            portNum  = atoi(argv[i]);
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
            argv2[1] = mount;
        }
    }
    
    return fuse_main(argc2, argv2, &snfs_oper, NULL);
}

int snfs_open(const char *path, struct fuse_file_info *fi){
		
	int connection = openConnection();
	if(connection < 0){
		h_errno = HOST_NOT_FOUND;
		return -1;
	}

	int command = htonl(0);

	if(send(connection, &command, sizeof(command), 0) < 0)
		printf("Error sending open command to the server\n");
	
    //send pathlen to server
    int pathLen  = strlen(path) + 1;
    int pathLength = htonl(pathLen);
    int numBytesSent = send(connection, &pathLength, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending pathlen to the server\n");

	//send path to the server    
    numBytesSent = send(connection, path, pathLen, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    
	//send flags to server
	int flagMessage = htonl(fi->flags);
	if(send(connection, &flagMessage, sizeof(flagMessage), 0) < 0)
		printf("Error sending flags to the server\n");
    
    //recv fd
	int output_fd;
	int out;
	if(recv(connection, &out, sizeof(out), 0) == -1){
		perror("Error receiving fd from the server\n");
		return -1;		
	}
	
    	output_fd = ntohl(out);
	fi->fh = output_fd;

	//receive any errors
	int errorMessage;
	if(output_fd == -1){
		if(recv(connection, &errorMessage, sizeof(errorMessage), 0) == -1){
			perror("Could not recieve error message from the server\n");
			return -1;
		}
		errno = ntohl(errorMessage);
        return -errno;
	}
	close(connection);
    
	return 0;	
}

int snfs_flush(const char *path, struct fuse_file_info *fi){
    (void) path;

    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(3);
    
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending flush command to the server\n");
        
    //send handle
    int fh = htonl(fi->fh);
    int numBytesSent = send(connection, &fh, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending fh to the server\n");

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
        return -errno;
    }

    close(connection);
    return result;
}

int snfs_release(const char *path, struct fuse_file_info *fi){
    (void) path;

    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(11);
    
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending release command to the server\n");
        
    //send handle
    int fh = htonl(fi->fh);
    int numBytesSent = send(connection, &fh, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending fh to the server\n");

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
        return -errno;
    }

    close(connection);
    return result;
}

int snfs_truncate(const char *path, off_t size){
    int connection = openConnection();

    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(5);

    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending truncate command to the server\n");
    
    int pathLen  = strlen(path) + 1;
    int pathLength = htonl(pathLen);
    int numBytesSent = send(connection, &pathLength, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending pathlen to the server\n");
    
    numBytesSent = send(connection, path, pathLen, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");

    //send size
    int sizeN = htonl(size);
    
    if(send(connection, &sizeN, sizeof(sizeN), 0) < 0)
        printf("Error sending size to the server\n");
    
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
        result = -errno;
    }
    close(connection);

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
        
    //send path to the server
    int pathLen  = strlen(path) + 1;
    int pathLength = htonl(pathLen);
    int numBytesSent = send(connection, &pathLength, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending pathlen to the server\n");
    
    numBytesSent = send(connection, path, pathLen, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
            
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
    (void) path;
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(1);

    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending read command to the server\n");
        
    //send fi
    int fh = htonl(fi->fh);
    int numBytesSent = send(connection, &fh, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
        
    //send size    
    int sizeN = htonl(size);
    if(send(connection, &sizeN, sizeof(int), 0) < 0)
        printf("Error sending size to the server\n");
        
    //send offset    
    int offN = htonl(offset);
    if(send(connection, &offN, sizeof(int), 0) < 0)
        printf("Error sending offset to the server\n");
        
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
        return -errno;
    }
    else
    {
        if(recvall(connection, buf, size) == -1){
            perror("Could not recieve buf from the server\n");
            return -1;
        }

    }
    close(connection);
    
    return result;
}

int snfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
    (void) path;
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(2);
    
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending write command to the server\n");
        
    //send handle
    int fh = htonl(fi->fh);
    int numBytesSent = send(connection, &fh, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending buf to the server\n");
        
    //send size
    int sizeN = htonl(size);
    
    if(send(connection, &sizeN, sizeof(sizeN), 0) < 0)
        printf("Error sending size to the server\n");
    
    //send buf
    numBytesSent = sendall(connection, buf, size);
    if(numBytesSent < 0)
        printf("Error sending buf to the server\n");
        
    //send offset
    int offsetN = htonl(offset);
    
    if(send(connection, &offsetN, sizeof(offsetN), 0) < 0)
        printf("Error sending offset to the server\n");
        
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
        return -errno;
    }
    close(connection);
     
    return result;
}

int snfs_opendir(const char *path, struct fuse_file_info *fi){
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(7);
    
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending read command to the server\n");
    
    //send path to the server
    int pathLen  = strlen(path) + 1;
    int pathLength = htonl(pathLen);
    int numBytesSent = send(connection, &pathLength, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending pathlen to the server\n");
    
    numBytesSent = send(connection, path, pathLen, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");

    //recv fd
    int output_fd;
    if(recv(connection, &output_fd, sizeof(output_fd), 0) == -1){
        perror("Error receiving fd from the server\n");
        return -1;      
    }

    int out = ntohl(output_fd);
    fi->fh = out;
    int errorMessage;
    if(out == -1){
        if(recv(connection, &errorMessage, sizeof(errorMessage), 0) == -1){
            perror("Could not recieve error message from the server\n");
            return -1;
        }
        errno = ntohl(errorMessage);
        return -errno;
    }
    printf("\n");
    close(connection);

    return 0;
}

int snfs_readdir(const char* path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){
    (void) offset;
    (void) path;

    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(8);
    
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending readdir command to the server\n");
        
    //send fh to the server
    int fd = htonl(fi->fh);
    int numBytesSent = send(connection, &fd, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending fh to the server\n");

    int x, xN;
    int y;
    int z;
    while(1)
    {
        recv(connection, &xN, sizeof(int), 0);
        x = ntohl(xN);

        if(x == -1) break;

        struct stat st;
        memset(&st, 0, sizeof(st));
        char *dname = (char*)malloc(x);
        recv(connection, dname, x, 0);
        recv(connection, &st, sizeof(struct stat), 0);
        if(filler(buf, dname, &st, 0)){
            y = -1;
            z = htonl(y);
            send(connection, &z, sizeof(int), 0);
            break;
        }
        else
        {
            y = 0;
            z = htonl(y);            
            send(connection, &z, sizeof(int), 0);
        }
	free(dname); 
    }
	
    if(x != -1) recv(connection, &x, sizeof(int), 0);
    close(connection);

    return 0;
}

int snfs_releasedir(const char *path, struct fuse_file_info *fi){
    (void) path;

    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(9);
    
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending releasedir command to the server\n");
        
    //send handle
    int fh = htonl(fi->fh);
    int numBytesSent = send(connection, &fh, sizeof(uint64_t), 0);
    if(numBytesSent < 0)
        printf("Error sending fh to the server\n");

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
        return -errno;
    }

    close(connection);
    return result;
}

int snfs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(4);
    
    if(send(connection, &command, sizeof(command), 0) < 0)
    {
        printf("Error sending create command to the server\n");
        return -1;
    }

    //send path to the server
    int pathLen  = strlen(path) + 1;
    int pathLength = htonl(pathLen);
    int numBytesSent = send(connection, &pathLength, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending pathlen to the server\n");
    
    numBytesSent = send(connection, path, pathLen, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");

    //send mode to server
    int modeN = htonl(mode);
    numBytesSent = send(connection, &modeN, sizeof(modeN), 0);
    if(numBytesSent < 0){
        printf("Error sending modeN to the server\n");
        return -1;
    }

    //recv fd
    int output_fd;
    if(recv(connection, &output_fd, sizeof(output_fd), 0) == -1){
        perror("Error receiving fd from the server\n");
        return -1;      
    }

    int out = ntohl(output_fd);
    fi->fh = out;

    int errorMessage;
    if(out == -1){
        if(recv(connection, &errorMessage, sizeof(errorMessage), 0) == -1){
            perror("Could not recieve error message from the server\n");
            return -1;
        }
        errno = ntohl(errorMessage);
        return -errno;
    }
    printf("\n");
    close(connection);

    return 0;
}

int snfs_mkdir(const char *path, mode_t mode){
    int connection = openConnection();
    
    if(connection < 0){
        h_errno = HOST_NOT_FOUND;
        return -1;
    }
    
    int command = htonl(10);
    
    if(send(connection, &command, sizeof(command), 0) < 0)
        printf("Error sending open command to the server\n");
        
    //send path to the server
    int pathLen  = strlen(path) + 1;
    int pathLength = htonl(pathLen);
    int numBytesSent = send(connection, &pathLength, sizeof(int), 0);
    if(numBytesSent < 0)
        printf("Error sending pathlen to the server\n");
    
    numBytesSent = send(connection, path, pathLen, 0);
    if(numBytesSent < 0)
        printf("Error sending path to the server\n");
    
    //send mode to server
    int modeN = htonl(mode);
    numBytesSent = send(connection, &modeN, sizeof(modeN), 0);
    if(numBytesSent < 0)
        printf("Error sending modeN to the server\n");
        
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
        return -errno;
    }
    close(connection);
    
    return result;
}

int openConnection(){
    int port = portNum;
    
    int clientSocket;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        printf("Failed to create the client socket. Error: %d\n",errno);
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    server = gethostbyname(address); //Update this later
    if (server == NULL) { //Not sure if I did this right?
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

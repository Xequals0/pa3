#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <pthread.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <dirent.h>
#include <fuse.h>

typedef struct{
	char ipAddress[INET_ADDRSTRLEN];
	int command;
	/*
     Corresponding command values:
     0. open
     1. read
     2. write
     3. flush
     4. create
     5. truncate
     6. getattr
     7. opendir
     8. readdir
     9. releasedir
     10. mkdir
     11. release
     */
    int fd;
} client_args;

typedef struct sockaddr SA;

void* selectMethod(void *arg);

char* mount;
int main(int argc, char* argv[])
{
    int port = -1;
    int mountarg = 0;
    int i;
    for(i = 1; i < argc; i++)
    {
        if(strcmp(argv[i], "-mount") == 0)
        {
            i++;
            mount = (char*)malloc(strlen(argv[i]) + 1);
            strcpy(mount, argv[i]);
            mountarg = 1;
        }
        else if(strcmp(argv[i], "-port") == 0)
        {
            i++;
            port = atoi(argv[i]);
        }
    }

  //  if(mountarg == 0) printf("No mount point specified!\n");
  // if(port == -1) printf("No port number specified!\n");
    if(mountarg == 0 || port == -1) return -1; 

    int serverSocket, *clientSocket;
	socklen_t clientlen = sizeof(struct sockaddr_storage);
	struct sockaddr_storage clientaddr;
	char client_hostname[8192], client_port[8192];
    struct sockaddr_in serv_addr;
	pthread_t tid;

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        printf("Failed to create the server socket. Error: %d\n",errno);
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port=htons(port);

    if(bind(serverSocket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Failed to bind. Error: %d\n",errno);
        exit(1);
    }

    if(listen(serverSocket, 100) < 0) //Arbitrarily chose 100 for the backlog
    {
        printf("Listen failed. Error: %d\n",errno);
        exit(1);
    }

	client_args *clientinfo = (client_args *)malloc(sizeof(client_args));
    do
    {
	clientlen = sizeof(struct sockaddr_storage);
        clientSocket = (int*)malloc(sizeof(int));
	*clientSocket = accept(serverSocket, (SA *)&clientaddr, &clientlen);
        if (*clientSocket < 0)
        {
            printf("Failed to accept a new connection. Error: %d\n",errno);
        }
	getnameinfo((SA *)&clientaddr, clientlen, client_hostname, 8192, client_port, 8192, 0);

	clientinfo->fd = *clientSocket;

	char hostIP[NI_MAXHOST];
	int rc = getnameinfo((struct sockaddr *)&clientaddr, clientlen, hostIP, sizeof(hostIP), NULL, 0 , NI_NUMERICHOST | NI_NUMERICSERV);

	if(rc == 0){
		strcpy(clientinfo->ipAddress, hostIP);
	}
	else
		perror("error getting the clients IP address\n");

	//get what command is being called, ex: open, close, etc
	int command = -1;
	if(recv(*clientSocket, &command, sizeof(command), 0) == -1){
		perror("Could not get the command from the client\n");
		exit(1);	
	}

	clientinfo->command = ntohl(command);

	pthread_create(&tid, NULL, selectMethod, (void*)clientinfo);	

    } while (1);

    close(serverSocket);
    return 0;
}

int server_open(client_args *client){
    int pathsize;
    int recv_file;

    if((recv_file = recv(client->fd, &pathsize, sizeof(int), 0)) == -1)
        perror("Error reading pathsize from the client\n");

    int pathLen = ntohl(pathsize);
    int fullLen = pathLen + strlen(mount);

    char filenameBuffer[fullLen];
    bzero(&filenameBuffer, sizeof(filenameBuffer));
    char *filename = (char *)malloc(fullLen);
    strcpy(filename, mount);

	if((recv_file = recv(client->fd, filenameBuffer, pathLen, 0)) == -1)
		perror("Error reading filename from the client\n");

	strcat(filename, filenameBuffer);

	int tmpFlag;

	if(recv(client->fd, &tmpFlag, sizeof(tmpFlag), 0) < 0)
		printf("Error reading flags from the client\n");

	int flag = ntohl(tmpFlag);
	
    int fd = open(filename, flag);
    int fdN = htonl(fd);

	if(send(client->fd, &fdN, sizeof(fdN), 0) == -1)
		perror("Error sending fd to the client");


	if(fd == -1){
            int error = htonl(errno);
		if(send(client->fd, &error, sizeof(error), 0) == -1){
			printf("Error sending errno to the client");
		}
	}

	return fd;
}

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

int server_read(client_args *client){
    
    //recv fi
    int fh, fhN;
    int recv_fi;

    if((recv_fi = recv(client->fd, &fhN, sizeof(fh), 0)) == -1)
        perror("Error reading path from the client\n");
    fh = ntohl(fhN);

    //recv size
    int size;
    int sizeN;
    if(recv(client->fd, &sizeN, sizeof(int), 0) < 0)
        printf("Error reading size from the client\n");
    size = ntohl(sizeN);        

    //recv offset
    off_t offset;
    int offsetN; 
    
    if(recv(client->fd, &offsetN, sizeof(int), 0) < 0)
        printf("Error reading offset from the client\n");
    offset = ntohl(offsetN);
    
    char *buf = (char*)malloc(size);
    int res = pread(fh, buf, size, offset);

    
    if (res == -1){
        int retVal = htonl(-1);
        
        //lets client know that an error has occured
        if(send(client->fd, &retVal, sizeof(retVal), 0) == -1)
            perror("Could not send -1 to the client\n");
        
        //send errno
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1)
            perror("Could not send errno to the client");
                
        return errno;
    }
    
    int result = htonl(res);
    
    //send result from pread call
    if(send(client->fd, &result, sizeof(result) , 0) == -1)
        perror("Could not send the write return value to the client\n");

    //send buffer
    if(sendall(client->fd, buf, size) == -1)
        perror("Could not send the read value to the client\n");
        
    return res;
}

int server_write(client_args *client){

    //receive handle  
    int fh;
    int fhN;

    int recv_fh;
    if((recv_fh = recv(client->fd, &fhN, sizeof(int), 0)) == -1)
        perror("Error reading path from the client\n");
    fh = ntohl(fhN);

    //recv size
    int sizeN;
    bzero(&sizeN, sizeof(sizeN));
    
    if(recv(client->fd, &sizeN, sizeof(sizeN), 0) < 0)
        printf("Error reading size from the client\n");
    
    int size = ntohl(sizeN);

    //recv buf
    char *buf = malloc(size);
    if(recvall(client->fd, buf, size) < 0)
        printf("Error reading size from the client\n");    
    
    //recv offset
    int offsetN;
    bzero(&offsetN, sizeof(offsetN));
    
    if(recv(client->fd, &offsetN, sizeof(offsetN), 0) < 0)
        printf("Error reading offset from the client\n");
    
    int offset = ntohl(offsetN);

    int res = pwrite(fh, buf, size, offset);
    if (res == -1){
        int retVal = htonl(-1);
        
        //lets client know that an error has occured
        if(send(client->fd, &retVal, sizeof(retVal), 0) == -1)
            perror("Could not send -1 to the client\n");
        
        //send errno
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1)
            perror("Could not send errno to the client");
        
        
        return errno;
    }
    
    int result = htonl(res);
    
    //send result from pwrite call
    if(send(client->fd, &result, sizeof(result) , 0) == -1)
        perror("Could not send the write return value to the client\n");
    
    return res;
}

int server_flush(client_args *client){
	int fds = -1;

	if(recv(client->fd, &fds, sizeof(fds), 0) == -1)
		perror("Could not receive fd to flush from the client");

	int fd = ntohl(fds);

    int result = close(dup(fd));
    int res = htonl(result);

    //send result
    if(send(client->fd, &res, sizeof(res) , 0) == -1)
        perror("Could not send the flush return value to the client\n");
    
    //send errno if there is an error
    if(result == -1){
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1)
            perror("Could not send errno to the client");
    }
    
    return result;
}

int server_release(client_args *client){
    int fds = -1;

    if(recv(client->fd, &fds, sizeof(fds), 0) == -1)
        perror("Could not receive fd to release from the client");

    int fd = ntohl(fds);

    int result = close(fd);
    int res = htonl(result);

    //send result
    if(send(client->fd, &res, sizeof(res) , 0) == -1)
        perror("Could not send the release return value to the client\n");
    
    //send errno if there is an error
    if(result == -1){
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1)
            perror("Could not send errno to the client");
    }
    
    return result;
}

int server_mkdir(client_args *client){
    
    //recv path
    int pathsize;
    int recv_file;

    if((recv_file = recv(client->fd, &pathsize, sizeof(int), 0)) == -1)
        perror("Error reading pathsize from the client\n");

    int pathLen = ntohl(pathsize);
    int fullLen = pathLen + strlen(mount);

    //recv path
    char pathBuffer[fullLen];
    bzero(&pathBuffer, sizeof(pathBuffer));
    char *path = (char *)malloc(fullLen);
    strcpy(path, mount);
    
    int recv_path;
    if((recv_path = recv(client->fd, pathBuffer, pathLen, 0)) == -1)
        perror("Error reading path from the client\n");
    
    strcat(path, pathBuffer);
    
    //recv mode
    int modeN;
    mode_t mode;

    if(recv(client->fd, &modeN, sizeof(modeN), 0) < 0)
        printf("Error reading mode from the client\n");
         
    int result;

    mode = ntohl(modeN);

    result = mkdir(path, mode);
    
    int mkdirResult = htonl(result);
    
    //send result from mkdir call
    if(send(client->fd, &mkdirResult, sizeof(mkdirResult) , 0) == -1)
        perror("Could not send the mkdir return value to the client\n");
    
    //send errno if there is an error
    if(result == -1){
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1)
            perror("Could not send errno to the client");
    }
    
    return result;
}

int server_getattr(client_args *client){

    int pathsize;
    int recv_file;

    if((recv_file = recv(client->fd, &pathsize, sizeof(int), 0)) == -1)
        perror("Error reading pathsize from the client\n");

    int pathLen = ntohl(pathsize);
    int fullLen = pathLen + strlen(mount);

    //recv path
    char pathBuffer[fullLen];
    bzero(&pathBuffer, sizeof(pathBuffer));
    char *path = (char *)malloc(fullLen);
    strcpy(path, mount);
    
    int recv_path;
    if((recv_path = recv(client->fd, pathBuffer, pathLen, 0)) == -1)
        perror("Error reading path from the client\n");
    
    strcat(path, pathBuffer);
    
    struct stat *stbuf = malloc(sizeof(struct stat));
    int result = lstat(path, stbuf);
    
    int res = htonl(result);
    
    //send result from lstat call
    if(send(client->fd, &res, sizeof(res) , 0) == -1)
        perror("Could not send the getattr return value to the client\n");
    
    //send errno if there is an error
    if(result == -1){
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1)
            perror("Could not send errno to the client");
    }
    else if(result == 0)
    {
        if(send(client->fd, stbuf, sizeof(struct stat), 0) == -1)
            perror("Could not send the  stat struct to the client\n");
    }
    
    return result;

}

int server_readdir(client_args *client){
    DIR *dp;
    struct dirent *de;
    int fd;
    int recv_file;

    if((recv_file = recv(client->fd, &fd, sizeof(int), 0)) == -1)
        perror("Error reading pathsize from the client\n");  

    int fh = ntohl(fd);

    dp = fdopendir(fh);
    
    int z;
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        int y = strlen(de->d_name) + 1;
        int yN = htonl(y); 
        send(client->fd, &yN, sizeof(int), 0);       
        send(client->fd, de->d_name, y, 0);
        send(client->fd, &st, sizeof(struct stat), 0);
        recv(client->fd, &y, sizeof(int), 0);
        z = ntohl(y);
        if(z == -1) break;
    }
    int x = htonl(-1);
    if(send(client->fd, &x, sizeof(int), 0) == -1)
        perror("Could not send the buf to the client\n");

    return 0;
}

int server_truncate(client_args *client){
    int pathsize;
    int recv_file;

    if((recv_file = recv(client->fd, &pathsize, sizeof(int), 0)) == -1)
        perror("Error reading pathsize from the client\n");

    int pathLen = ntohl(pathsize);
    int fullLen = pathLen + strlen(mount);

    //recv path
    char pathBuffer[fullLen];
    bzero(&pathBuffer, sizeof(pathBuffer));
    char *path = (char *)malloc(fullLen);
    strcpy(path, mount);
    
    int recv_path;
    if((recv_path = recv(client->fd, pathBuffer, pathLen, 0)) == -1)
        perror("Error reading path from the client\n");
    
    strcat(path, pathBuffer);   

    int sizeN;
    bzero(&sizeN, sizeof(sizeN));
    
    if(recv(client->fd, &sizeN, sizeof(sizeN), 0) < 0)
        printf("Error reading size from the client\n");
    
    int size = ntohl(sizeN);

    int result = truncate(path, size);

    int res = htonl(result);
    
    //send result from lstat call
    if(send(client->fd, &res, sizeof(res) , 0) == -1)
        perror("Could not send the truncate return value to the client\n");
    
    //send errno if there is an error
    if(result == -1){
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1)
            perror("Could not send errno to the client");
    }

    return result;
}

int server_opendir(client_args *client){
    int pathsize;
    int recv_file;

    if((recv_file = recv(client->fd, &pathsize, sizeof(int), 0)) == -1)
        perror("Error reading pathsize from the client\n");

    int pathLen = ntohl(pathsize);
    int fullLen = pathLen + strlen(mount);

    //recv path
    char pathBuffer[fullLen];
    bzero(&pathBuffer, sizeof(pathBuffer));
    char *path = (char *)malloc(fullLen);
    strcpy(path, mount);
    
    int recv_path;
    if((recv_path = recv(client->fd, pathBuffer, pathLen, 0)) == -1)
        perror("Error reading path from the client\n");
    
    strcat(path, pathBuffer);    
    
    DIR *dir = opendir(path);
    if(dir == NULL){   //CHECK IF THIS STUFF IS RIGHT
        perror("Unable to open the requested directory.");

        int x = htonl(-1);
        if(send(client->fd, &x, sizeof(x), 0) == -1){
            perror("Error sending -1 to the client");
            return -1;
        }

        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1){
            printf("Error sending errno to the client");
            return -1;
        }

        return -1;
    }
    else
    {
        int fd = dirfd(dir);
        int fd_send = htonl(fd);
        if(send(client->fd, &fd_send, sizeof(fd), 0) == -1)
        {
            perror("Error sending fd to the client");
            return -1;
        }
    }
    return 0;
}

int server_releasedir(client_args *client){
    int fds = -1;
    DIR *dp;

    if(recv(client->fd, &fds, sizeof(fds), 0) == -1)
        perror("Could not receive fd to release from the client");

    int fd = ntohl(fds);

    dp = fdopendir(fd);
    int result = closedir(dp);
    
    int res = htonl(result);

    //send result
    if(send(client->fd, &res, sizeof(res) , 0) == -1)
        perror("Could not send the release return value to the client\n");
    
    //send errno if there is an error
    if(result == -1){
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1)
            perror("Could not send errno to the client");
    }
    
    return result;
}

int server_create(client_args *client){
    int pathsize;
    int recv_file;

    if((recv_file = recv(client->fd, &pathsize, sizeof(int), 0)) == -1)
        perror("Error reading pathsize from the client\n");

    int pathLen = ntohl(pathsize);
    int fullLen = pathLen + strlen(mount);

    //recv path
    char pathBuffer[fullLen];
    bzero(&pathBuffer, sizeof(pathBuffer));
    char *path = (char *)malloc(fullLen);
    strcpy(path, mount);
    
    int recv_path;
    if((recv_path = recv(client->fd, pathBuffer, pathLen, 0)) == -1)
        perror("Error reading path from the client\n");
    
    strcat(path, pathBuffer);

    //recv mode
    mode_t mode;
    int modeN;
    
    if(recv(client->fd, &modeN, sizeof(modeN), 0) < 0)
        printf("Error reading mode from the client\n");
    
    mode = ntohl(modeN);
        
   int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, mode);
   int send_fd = htonl(fd);

    if(send(client->fd, &send_fd, sizeof(fd), 0) == -1)
        perror("Error sending fd to the client");

    if(fd == -1){
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1){
            printf("Error sending errno to the client");
        }
    }

    return fd;    

}

void* selectMethod(void *arg){
	client_args *args = (client_args *)arg;
	int command = args->command;
	
	if(command == 0){
		server_open(args);
	}
	else if(command == 1){ //read
        server_read(args);
    }
	else if(command == 2){ //write
        server_write(args);
    }
	else if(command == 3){
		server_flush(args);
	}
    else if(command == 4){ //create
        server_create(args);
    }
    else if(command == 5){ //truncate
        server_truncate(args);
    }
    else if(command == 6){ //getattr
        server_getattr(args);
    }
    else if(command == 7){ //opendir
        server_opendir(args);
    }
    else if(command == 8){ //readdir
        server_readdir(args);
    }
    else if(command == 9){ //releasedir
        server_releasedir(args);
    }
    else if(command == 10){ //mkdir
        server_mkdir(args);
    }
    else if(command == 11){ //mkdir
        server_release(args);
    }
    
    return NULL;	
}

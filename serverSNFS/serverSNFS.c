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

typedef struct File{
	char *pathname;
	int fd;
	int flags;
	struct File *next;
} fileNode;

typedef struct{
	char ipAddress[INET_ADDRSTRLEN];
	int command;
	/*
     Corresponding command values:
     0. open
     1. read
     2. write
     3. close
     4. create
     5. truncate
     6. getattr
     7. opendir
     8. readdir
     9. releasedir
     10. mkdir
     */
    int fd;
	fileNode *database;
	int isEmptyDatabase;
} client_args;

typedef struct Node{
	client_args *client;
	struct Node *next;
} node;

typedef struct sockaddr SA;

fileNode* searchDataBaseFileName(client_args *client, char *filename);
fileNode* searchDataBaseFD(client_args *client, int fd);
int removeFileNode(client_args *client, int fd);
void* selectMethod(void *arg);
node* searchListAddress(char *ipAddress);
int addClientNode(client_args *client);


node* list = NULL;

int numReaders;
int isWriting;
pthread_mutex_t mutex;
pthread_cond_t finishedWriting;
pthread_cond_t finishedReading;

int main()//(int argc, const char* argv[])
{
	//char* port = argv[1];
    int port = 13175; //This is temporary; We should obtain this from the user.

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
	clientinfo->database = NULL;
	clientinfo->isEmptyDatabase = 1;
    do
    {
	clientlen = sizeof(struct sockaddr_storage);
        clientSocket = (int*)malloc(sizeof(int));
	*clientSocket = accept(serverSocket, (SA *)&clientaddr, &clientlen);
        if (*clientSocket < 0)
        {
            printf("Failed to accept a new connection. Error: %d\n",errno);
        }/*
        else
        {
            pthread_t tid;
            pthread_create(&tid, NULL, client_thread_handler, &clientSocket);
        }*/

	getnameinfo((SA *)&clientaddr, clientlen, client_hostname, 8192, client_port, 8192, 0);

	clientinfo->fd = *clientSocket;

	char hostIP[NI_MAXHOST];
	int rc = getnameinfo((struct sockaddr *)&clientaddr, clientlen, hostIP, sizeof(hostIP), NULL, 0 , NI_NUMERICHOST | NI_NUMERICSERV);

	if(rc == 0){
		strcpy(clientinfo->ipAddress, hostIP);
	}
	else
		perror("error getting the clients IP address\n");

	node *newClientNode = (node *) malloc(sizeof(node));
	newClientNode->client = clientinfo;
	newClientNode->next = NULL;
	if(list == NULL)
		list = newClientNode;

	node *searchList = searchListAddress(clientinfo->ipAddress);
	if(strcmp(searchList->client->ipAddress, clientinfo->ipAddress) == 0){
		//already in list
	}
	else if(searchList->next == NULL && strcmp(searchList->client->ipAddress, clientinfo->ipAddress) != 0)
		addClientNode(clientinfo);

	//get what command is being called, ex: open, close, etc
	int command = -1;
	if(recv(*clientSocket, &command, sizeof(command), 0) == -1){
		perror("Could not get the command from the client\n");
		exit(1);	
	}

	clientinfo->command = ntohl(command);
	printf("command is: %d\n", clientinfo->command);

	pthread_create(&tid, NULL, selectMethod, (void*)clientinfo);	

    } while (1);

    close(serverSocket);
    return 0;
}


void* client_thread_handler(void* clientSock)
{
    printf("Hey there!\n");
    char buffer[256] = "Hey there!\n";
    //Send Welcome (this is a test)
    send(*(int*)clientSock, buffer, sizeof(buffer), 0);

    //Fill in the rest of the handler here

    printf("A connection has closed\n");
    pthread_exit(0);
}


int removeFileNode(client_args *client, int fd){
	if(client->database == NULL || client->isEmptyDatabase == 1)
		return -1;

	//not sure if we need this
	/*if(client->database->next == NULL){
		client->database = NULL;
		client->isEmptyDatabase = 1;
		return 0;
	} */

	fileNode *temp = client->database, *prev;
	if(temp->fd == fd){
		client->database = temp->next;
		free(temp);
		return 0;
	}

	int count = 1;
	while(temp != NULL && (temp->fd != fd)){
		count++;
		prev = temp;
		temp = temp->next;
	}

	if(temp == NULL){ //reached end of list
		return -1;
	}

	prev->next = temp->next;

	free(temp);

	return 0;
}

fileNode* searchDataBaseFD(client_args *client, int fd){
	fileNode *ptr = client->database;
	fileNode *prv = NULL;

	if(ptr == NULL)
		return NULL;

	while(ptr != NULL){
		if(ptr->fd == fd)
			return ptr;
		prv = ptr;
		ptr = ptr->next;
	}

	return prv;
}

fileNode* searchDataBaseFileName(client_args *client, char *filename){
	fileNode *ptr = client->database;
	fileNode *prv = NULL;

	if(client->isEmptyDatabase == 1){
		client->isEmptyDatabase = 0; // double check this
 		return NULL;
	}

	while(ptr != NULL){
		if(strcmp(ptr->pathname, filename) == 0)
			return ptr;
		prv = ptr;
		ptr = ptr->next;
	}

	return prv;
}


int server_open(client_args *client){
	char filenameBuffer[255];
	bzero(&filenameBuffer, sizeof(filenameBuffer));
	char *filename = (char *)malloc(sizeof(char));

	int recv_file;
	if((recv_file = recv(client->fd, filenameBuffer, sizeof(filenameBuffer), 0)) == -1)
		perror("Error reading filename from the client\n");

	/*fileNode* searchDatabase;
	fileNode* newFile = (fileNode *) malloc(sizeof(fileNode));
	int isLast = 0;
	int fileExists = 0;*/

	filename[recv_file] = '\0';
	strcpy(filename, filenameBuffer);

 	/*searchDatabase = searchDataBaseFileName(client, filename);

	newFile->pathname = filename;
	
	printf("filename: %s\n", filename);

	if(searchDatabase != NULL){
		if(strcmp(searchDatabase->pathname, filename) == 0)
			fileExists = 1;
		else if(strcmp(searchDatabase->pathname, filename) != 0 && searchDatabase->next == NULL){
			isLast = 1;
		}
	}
     */
	int tmpFlag;
	bzero(&tmpFlag, sizeof(tmpFlag));

	if(recv(client->fd, &tmpFlag, sizeof(tmpFlag), 0) < 0)
		printf("Error reading flags from the client\n");

	int flag = ntohl(tmpFlag);
	printf("flag: %d\n", flag);

/*	if(isLast == 1)
		newFile->flags = flag;
*/

/*    if(flag == 0)
        fd = open(filename, O_RDONLY);
    else if(flag == 1)
        fd = open(filename, O_WRONLY);
    else if(flag == 2)
        fd = open(filename, O_RDWR);
    else
        fd = -1;

	printf("fd: %d\n", fd);

	if(fd != -1){
		newFile->fd = fd;
		newFile->next = NULL;

		if(client->database == NULL)
			client->database = newFile;

		if(isLast == 1)
			searchDatabase->next = newFile;
	}
    */
    
    int fd = open(filename, flags);
    
	if(send(client->fd, &fd, sizeof(fd), 0) == -1)
		perror("Error sending fd to the client");


	if(fd == -1){
            int error = htonl(errno);
		if(send(client->fd, &error, sizeof(error), 0) == -1){
			printf("Error sending errno to the client");
		}
	}

	return fd;
}

int server_read(client_args *client){
    
    //recv path
    char pathBuffer[256];
    bzero(&pathBuffer, sizeof(pathBuffer));
    char *path = (char *)malloc(sizeof(char));
    
    int recv_path;
    if((recv_path = recv(client->fd, pathBuffer, sizeof(pathBuffer), 0)) == -1)
        perror("Error reading path from the client\n");
    
    path[recv_path] = '\0';
    strcpy(path, pathBuffer);
    
    //recv buf
    char bufBuffer[256];
    bzero(&bufBuffer, sizeof(bufBuffer));
    char *buf = (char *)malloc(sizeof(char));
    
    int recv_buf;
    if((recv_buf = recv(client->fd, bufBuffer, sizeof(bufBuffer), 0)) == -1)
        perror("Error reading buf from the client\n");
    
    path[recv_buf] = '\0';
    strcpy(buf, bufBuffer);
    
    //recv size
    int sizeN;
    bzero(&sizeN, sizeof(sizeN));
    
    if(recv(client->fd, &sizeN, sizeof(sizeN), 0) < 0)
        printf("Error reading size from the client\n");
    
    int size = ntohl(sizeN);
    
    //recv offset
    int offsetN;
    bzero(&offsetN, sizeof(offsetN));
    
    if(recv(client->fd, &offsetN, sizeof(offsetN), 0) < 0)
        printf("Error reading offset from the client\n");
    
    int offset = ntohl(offsetN);
    
    
    int fd = open(path, O_RDONLY);
    
    if (fd == -1){
        
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
    
    int res = pread(fd, buf, size, offset);
    
    if (res == -1){
        int retVal = htonl(-1);
        
        //lets client know that an error has occured
        if(send(client->fd, &retVal, sizeof(retVal), 0) == -1)
            perror("Could not send -1 to the client\n");
        
        //send errno
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1)
            perror("Could not send errno to the client");
        
        
        close(fd);
        
        return errno;
    }
    
    int result = htonl(res);
    
    //send result from pwrite call
    if(send(client->fd, &result, sizeof(result) , 0) == -1)
        perror("Could not send the write return value to the client\n");
    
    close(fd);
    
    return res;
}

int server_write(client_args *client){
    
    //recv path
    char pathBuffer[256];
    bzero(&pathBuffer, sizeof(pathBuffer));
    char *path = (char *)malloc(sizeof(char));
    
    int recv_path;
    if((recv_path = recv(client->fd, pathBuffer, sizeof(pathBuffer), 0)) == -1)
        perror("Error reading path from the client\n");
    
    path[recv_path] = '\0';
    strcpy(path, pathBuffer);
    
    //recv buf
    char bufBuffer[256];
    bzero(&bufBuffer, sizeof(bufBuffer));
    char *buf = (char *)malloc(sizeof(char));
    
    int recv_buf;
    if((recv_buf = recv(client->fd, bufBuffer, sizeof(bufBuffer), 0)) == -1)
        perror("Error reading buf from the client\n");
    
    path[recv_buf] = '\0';
    strcpy(buf, bufBuffer);
    
    //recv size
    int sizeN;
    bzero(&sizeN, sizeof(sizeN));
    
    if(recv(client->fd, &sizeN, sizeof(sizeN), 0) < 0)
        printf("Error reading size from the client\n");
    
    int size = ntohl(sizeN);
    
    //recv offset
    int offsetN;
    bzero(&offsetN, sizeof(offsetN));
    
    if(recv(client->fd, &offsetN, sizeof(offsetN), 0) < 0)
        printf("Error reading offset from the client\n");
    
    int offset = ntohl(offsetN);
    
    
    int fd = open(path, O_WRONLY);
    if (fd == -1){
        
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

    int res = pwrite(fd, buf, size, offset);
    if (res == -1){
        int retVal = htonl(-1);
        
        //lets client know that an error has occured
        if(send(client->fd, &retVal, sizeof(retVal), 0) == -1)
            perror("Could not send -1 to the client\n");
        
        //send errno
        int error = htonl(errno);
        if(send(client->fd, &error, sizeof(error), 0) == -1)
            perror("Could not send errno to the client");
        
        
        close(fd);
        
        return errno;
    }
    
    int result = htonl(res);
    
    //send result from pwrite call
    if(send(client->fd, &result, sizeof(result) , 0) == -1)
        perror("Could not send the write return value to the client\n");
    
    close(fd);
    
    return res;
}

int server_close(client_args *client){
	int fds = -1;

	if(recv(client->fd, &fds, sizeof(fds), 0) == -1)
		perror("Could not receive fd to close from the client");

	int fd = ntohl(fds);
	printf("fd going to be closed is %d\n", fd);

	int closed; //fileExists = 0;

	fileNode *searchDatabase = searchDataBaseFD(client, fd);
	if(searchDatabase == NULL || (searchDatabase->next == NULL && searchDatabase->fd != fd)){
		int err = htonl(EBADF);
		int retVal = htonl(-1);
		if(send(client->fd, &retVal, sizeof(retVal), 0) == -1)
			perror("Could not send return value to the client\n");

		sleep(1);

		if(send(client->fd, &err, sizeof(err), 0) == -1)
			perror("Could not send errno to the client\n");

		return -1;
	}

	if(searchDatabase->fd == fd){
		//fileExists  = 1;
		pthread_mutex_lock(&mutex);
		if(numReaders== 0 && isWriting == 0)
			closed = close(fd);
		else
			closed = 0;
		pthread_mutex_unlock(&mutex);
	}
	else{ // file is not in the database
		closed = -1;
	}

	int closeResult = htonl(closed);

	if(send(client->fd, &closeResult, sizeof(closeResult) , 0) == -1)
		perror("Could not send the close return value to the client\n");

	if(closed == -1){
		errno = 9;
		int error = htonl(errno);
		if(send(client->fd, &error, sizeof(error), 0) == -1)
			perror("Could not send errno to the client");
	}
	else{
		removeFileNode(client, fd);
	}

	return closed;
}

int server_mkdir(client_args *client){
    
    //recv path
    char pathBuffer[256];
    bzero(&pathBuffer, sizeof(pathBuffer));
    char *path = (char *)malloc(sizeof(char));
    
    int recv_path;
    if((recv_path = recv(client->fd, pathBuffer, sizeof(pathBuffer), 0)) == -1)
        perror("Error reading path from the client\n");
    
    path[recv_path] = '\0';
    strcpy(path, pathBuffer);
    
    //recv mode
    int modeN;
    bzero(&modeN, sizeof(modeN));
    
    if(recv(client->fd, &modeN, sizeof(modeN), 0) < 0)
        printf("Error reading mode from the client\n");
    
    int mode = ntohl(modeN);
    
    int result;
    
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

int snfs_getattr(client_args *client){
    //recv path
    char pathBuffer[256];
    bzero(&pathBuffer, sizeof(pathBuffer));
    char *path = (char *)malloc(sizeof(char));
    
    int recv_path;
    if((recv_path = recv(client->fd, pathBuffer, sizeof(pathBuffer), 0)) == -1)
        perror("Error reading path from the client\n");
    
    path[recv_path] = '\0';
    strcpy(path, pathBuffer);
    
    //recv stbuf
    
    
    
    
    
    
    
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
    
    return result;

}



void* selectMethod(void *arg){
	client_args *args = (client_args *)arg;
	int command = args->command;
	
	if(command == 0){
		printf("calling server_open\n");
		server_open(args);
	}
	else if(command == 1){ //read
        printf("calling server_read");
        server_read(args);
    }
	else if(command == 2){ //write
        printf("calling server_write");
        server_close(args);
    }
	else if(command == 3){
		printf("calling server_close");
		server_close(args);
	}
    else if(command == 4){ //create
        
    }
    else if(command == 5){ //truncate
        
    }
    else if(command == 6){ //getattr
        
    }
    else if(command == 7){ //opendir
        
    }
    else if(command == 8){ //readdir
        
    }
    else if(command == 9){ //releasedir
        
    }
    else if(command == 10){ //mkdir
        printf("calling server_mkdir");
        server_mkdir(args);
    }
	else{
		printf("No valid method was called\n");	
	}
	return NULL;	
}

node* searchListAddress(char *ipAddress){
	node *ptr = list;
	node *prv = NULL;

	if(ptr == NULL)return NULL;

	while(ptr != NULL){
		if(strcmp((ptr->client)->ipAddress, ipAddress) == 0)
			return ptr;
		prv = ptr;
		ptr = ptr->next;
	}
	return prv;
}

int addClientNode(client_args *client){
	node *doesExist = searchListAddress(client->ipAddress);
	node *newNode = (node *)malloc(sizeof(node));
	newNode->client = client;
	newNode->client->database = NULL;
	newNode->next = NULL;

	if(doesExist == NULL){
		list = newNode;
		return 0;
	}
	else if(doesExist->next == NULL && strcmp((doesExist->client)->ipAddress, client->ipAddress) != 0){
		doesExist->next = newNode;
		return 0;
	}
	else
		return -1;
}

































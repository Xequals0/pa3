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



int main(int argc, const char* argv[])
{
    int port = 13175; //This is temporary; We should obtain this from the user.
    
    int serverSocket, clientSocket;
    struct sockaddr_in serv_addr;
    
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
    
    do
    {
        clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket < 0)
        {
            printf("Failed to accept a new connection. Error: %d\n",errno);
        }
        else
        {
            pthread_t tid;
            pthread_create(&tid, NULL, client_thread_handler, &clientSocket);
        }
    } while (1);

    close(serverSocket);
    return 0;
}

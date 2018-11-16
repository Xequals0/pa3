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

int main(int argc, const char* argv[])
{
    int port = 13175; //This is temporary; We should obtain this from the user.
    
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
    
    char buffer[256];
    int receive;
    receive = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (receive < 0) {
        printf("Failed to receive from the server. Error: %d\n",errno);
        exit(1);
    } else {
        printf("%s", buffer);
    }
    
    //Continue with the client protocol here (call additional functions, etc.):
    close(clientSocket);
    return 0;
}

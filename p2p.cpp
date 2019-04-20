//#include <ifaddrs.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <iostream>
#include <string.h>

using namespace std;

int client(){

  struct sockaddr_in address;
  int sock = 0, valRead;
  int tempPort = 12345;
  fd_set readSet, writeSet;
  struct timeVal

  struct sockaddr_in servAddr;
  const char *message = "client Message";
  char buffer[1024];

  sock = socket(AF_INET, SOCK_STREAM, 0);

  memset(&servAddr, '0', sizeof(servAddr));

  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(tempPort);

  //Convert IPv4 addresses from text to binary form
  inet_pton(AF_INET, "127.0.0.1", &servAddr.sin_addr);

  connect(sock, (struct sockaddr *)&servAddr, sizeof(servAddr));

  send(sock , message , strlen(message) , 0 );
  valRead = read(sock, buffer, 1024);
  printf("%s\n",buffer );
  return 0;
}

int server(){
    int serverFd, newSocket, valRead;
    struct sockaddr_in address;
    int tempPort = 12345;
    int opt = 1;
    int addrlen = sizeof(address);

    //arbitrary size - 1 kb was what we wanted to do per round so ¯\_(ツ)_/¯
    char buffer[1024];

    const char *message = "responseConfirm";

    //Creating socket file descriptor
    serverFd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;

    //this should be dynamic for every incoming message
    address.sin_port = htons(tempPort);

    //Attach socket to the port
    if (bind(serverFd, (struct sockaddr *)&address, sizeof(address)) == -1){
      printf("Bad! \n");
    }

    listen(serverFd, 3);
    printf("accepting and reading \n");
    newSocket = accept(serverFd, (struct sockaddr *)&address,(socklen_t*)&addrlen);
    valRead = read(newSocket, buffer, 1024);

    printf("%s\n",buffer );

    send(newSocket, message, strlen(message) , 0 );
    printf("message sent\n");
    return 0;
}

int main(int argc, char *argv[]){
  if (argc!=2){
    printf("That's bad fam\n");
  }else if(strcmp(argv[1],"c") == 0){
    printf("Starting client\n");
    client();
  }else if(strcmp(argv[1],"s") == 0){
    printf("Starting server\n");
    server();
  }
  return 1;
}

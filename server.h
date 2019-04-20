# ifndef SERVER_H
# define SERVER_H

void* start_server(void* arg);
int server();

void* start_server(void* arg) {
  //printf("START SERVER\n");
  server();
  pthread_exit(NULL);
}

int server() {

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
    //return 0;
    pthread_exit(NULL);
}

# endif /* SERVER_H */

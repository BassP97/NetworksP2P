#ifndef CLIENT_H
#define CLIENT_H

void* start_client (void* arg);
int client();

void* start_client(void* arg)
{
  //printf("STARING CLIENT\n");
  client();
  pthread_exit(NULL);
}

int client() {
  struct sockaddr_in address;
  int sock = 0, valRead;
  int tempPort = 12345;
  fd_set readSet, writeSet;
  struct timeVal;

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

#endif /* CLIENT_H */

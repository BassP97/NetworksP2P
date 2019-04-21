#ifndef CLIENT_H
#define CLIENT_H

using namespace std;

void* start_client (void* arg);
int client(void);

void* start_client(void* arg)
{
  client();
  pthread_exit(NULL);
}

int client(void) {
  // TODO: eventually this will NOT exit after a single message exchange.
  struct sockaddr_in address;
  int sock = 0, valRead;
  int tempPort = 8080;
  fd_set readSet, writeSet;
  struct timeVal;

  struct sockaddr_in servAddr;
  //const char *message = "message from Hayley's computer";
  char message[64];
  char buffer[1024];
  char addr[8];

  // lets the user specify the other host to connect to; will eventually
  // be something that lets the user specify what file they want
  // the specified host must be running an instance of this program for this to work
  printf("Type in the address to send a message to:\n");
  cin >> addr;

  sock = socket(AF_INET, SOCK_STREAM, 0);

  memset(&servAddr, '0', sizeof(servAddr));

  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(tempPort);

  //Convert IPv4 addresses from text to binary form
  inet_pton(AF_INET, addr, &servAddr.sin_addr);

  // TODO: we shouldn't actually exist if a client can't connect somewhere
  if (connect(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) == -1)
  {
    perror("connect");
    return -1;
  }

  printf("Type in the message to send:\n");
  cin >> message;

  int sent = send(sock, message, strlen(message), 0);
  if (sent == -1) {
    perror("send");
  }
  //close(sock);
  // valRead = read(sock, buffer, 1024);
  // printf("%s\n",buffer );
  return 0;
}

#endif /* CLIENT_H */

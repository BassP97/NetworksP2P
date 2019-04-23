#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"
typedef unsigned char *byte_pointer;

using namespace std;

void* start_client (void* arg);
int client(void);
int writeToFile(struct fileReturn* toWrite, string fileName);
void showBytes(byte_pointer start, size_t len);

/* -----------------------------------------------------------------------------
 * void* start_client (void* arg)
 * Function for the client thread to start in. Calls client() where the client
 * initializes and handles input from the human user of the program and messages
 * from the servers.
 * Parameters:
 * - void* arg: a void pointer required by pthread_create(). Not used.
 * Returns: nothing
 * ---------------------------------------------------------------------------*/
void* start_client(void* arg)
{
  client();
  pthread_exit(NULL);
}

// TODO: docstring for this once it's done and we know exactly what it does
int client(void) {
  // TODO: eventually this will NOT exit after a single message exchange.
  struct sockaddr_in address;
  int sock = 0, valRead;
  int tempPort = 8080;
  fd_set readSet, writeSet;
  string fileName;
  struct timeVal;
  struct fileReturn* serverReturn;

  struct sockaddr_in servAddr;

  //chaged message to 128 to match max file name
  char* message;
  char rawBuffer[1024];
  char addr[8];

  // lets the user specify the other host to connect to; will eventually
  // be something that lets the user specify what file they want
  // the specified host must be running an instance of this program for this to work
  printf("Type in the address to send a message to:\n");
  std::cin >> addr;
  printf("trying to connect to %s\n",addr );

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
  printf("Successfully connected to %s\n",addr);

  // request a file from the server
  printf("Type in a filename to request from the server\n");
  cin >> fileName;
  // conver the file name to a cstring
  char* fileNameArr = new char[fileName.length()+1];
  strcpy(fileNameArr, fileName.c_str());

  // create a request
  struct fileRequest toRequest;
  strcpy(toRequest.fileName, fileNameArr);
  toRequest.portionToReturn = 0;

  printf("Requesting file with name %s\n",toRequest.fileName);
  printf("Requesting block number %ld\n",toRequest.portionToReturn);

  message = (char*)&fileNameArr;
  //janky, janky solution - fix later?
  message = message+41;
  showBytes((byte_pointer)message, sizeof(fileRequest));
  printf("\n");
  showBytes((byte_pointer)&toRequest, sizeof(fileRequest));

  // send the request
  int sent = send(sock, message, sizeof(fileRequest), 0);
  if (sent == -1) {
    perror("send");
  }

  valRead = read(sock, rawBuffer, sizeof(fileReturn));
  if (valRead == -1){
    perror("read");
  }

  serverReturn = (struct fileReturn*)rawBuffer;
  printf("Recieved data\n");
  showBytes((byte_pointer)rawBuffer, (size_t)1024);
  serverReturn = (struct fileReturn*)rawBuffer;

  printf("Data after processing:\n");
  showBytes((byte_pointer)serverReturn->data, size_t(serverReturn->bytesToUse));

  if(writeToFile(serverReturn, fileName)){
    printf("successfully wrote to %s\n", fileName.c_str());
  }else{
    printf("failed to write to file\n");
  }

  close(sock);
  delete[] fileNameArr;
  return 0;

}

int writeToFile(struct fileReturn* toWrite, string fileName){
  if(access( fileName.c_str(), F_OK ) == -1){
    ofstream writeFile(fileName, ios::out | ios::binary);
    if(writeFile.write(toWrite->data, toWrite->bytesToUse)){
        return 1;
    }else{
      return -1;
    }
  }
  return 1;
}

#endif /* CLIENT_H */

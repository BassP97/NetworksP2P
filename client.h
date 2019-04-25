#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"
typedef unsigned char *byte_pointer;
#define PORT 8080

using namespace std;

pthread_mutex_t client_fd_lock;
vector<int> client_fd_list;
vector<string> connected_list;

void* start_client_requester(void* arg);
int client_requester(void);
int client_connector(void);
int writeToFile(struct serverMessage* toWrite, string fileName);
void showBytes(byte_pointer start, size_t len);
vector<string> read_hosts(string filename);

/* -----------------------------------------------------------------------------
 * void* start_client_requester (void* arg)
 * Function for the client requester thread to start in. Calls client_requester()
 * which listens for user input on the command line, sends requests to servers,
 * and processes the servers' responses.
 * Parameters:
 * - void* arg: a void pointer required by pthread_create(). Not used.
 * Returns: nothing
 * ---------------------------------------------------------------------------*/
void* start_client_requester(void* arg)
{
  client_requester();
  pthread_exit(NULL);
}


/* -----------------------------------------------------------------------------
 * void* start_client_connector(void* arg)
 * Function for the client connector thread to start in. Reads the list of known
 * hosts, and repeatedly tries to connect to them. If a connection is successful
 * we add its socket to the list of shared sockets with the client requester
 * thread so that we can use it.
 * Parameters:
 * - void* arg: a void pointer required by pthread_create(). Not used.
 * Returns: nothing
 * ---------------------------------------------------------------------------*/
void* start_client_connector(void* arg)
{
  client_connector();
  pthread_exit(NULL);
}

// TODO: docstring for this once it's done and we know exactly what it does
int client_requester (void) {
  string fileName;
  char* message;
  int sock = 0, valRead;
  //changed message to 128 to match max file name
  char rawBuffer[sizeof(serverMessage)];
  struct serverMessage* serverReturn;

  // request a file from the server
  printf("Type in a filename to request from the server\n");
  cin >> fileName;
  //conver the file name to a cstring
  char* fileNameArr = new char[fileName.length()+1];
  strcpy(fileNameArr, fileName.c_str());
  //
  // create a request
  struct clientMessage toRequest;
  strcpy(toRequest.fileName, fileNameArr);
  toRequest.portionToReturn = 0;
  //
  printf("Requesting file with name %s\n",toRequest.fileName);
  printf("Requesting block number %ld\n",toRequest.portionToReturn);
  //
  message = (char*)&toRequest;
  showBytes((byte_pointer)message, sizeof(clientMessage));
  printf("\n");
  showBytes((byte_pointer)&toRequest, sizeof(clientMessage));

  // send the request
  int sent = send(sock, message, sizeof(clientMessage), 0);
  if (sent == -1) {
    perror("send");
  }

  valRead = read(sock, rawBuffer, sizeof(serverMessage));
  if (valRead == -1){
    perror("read");
  }

  serverReturn = (struct serverMessage*)rawBuffer;
  printf("Recieved data\n");
  showBytes((byte_pointer)rawBuffer, sizeof(serverMessage));
  serverReturn = (struct serverMessage*)rawBuffer;

  //printf("Data after processing:\n");
  //showBytes((byte_pointer)serverReturn->data, size_t(serverReturn->bytesToUse));

  printf("Data parameters \nFile size: %li \nPosition in file: %li\nBytes to use %i\n",
  serverReturn->fileSize, serverReturn->positionInFile, serverReturn->bytesToUse);

  if(writeToFile(serverReturn, fileName)){
    printf("successfully wrote to %s\n", fileName.c_str());
  }else{
    printf("failed to write to file\n");
  }

  close(sock);
  delete[] fileNameArr;
  return 0;

}

/* -----------------------------------------------------------------------------
 * int client_connector (void)
 * Function that the client connector thread runs in. Given a list of known
 * hosts that might also be running the program, goes through them and attempts
 * to make connections. If a connection is successful, we save the socket fd
 * associated with it so that the client requester thread can use that connect-
 * ion to request data from a server. If the connection is not successful,
 * close the file descriptor. Loops infinitely, sleeping between each loop. It
 * is necessary to continue to loop even if we have connected to all of the
 * open servers because more may open OR one may close and then open back up
 * again later.
 * Parameters: none
 * Returns: nothing right now
 * ---------------------------------------------------------------------------*/
int client_connector (void) {
  struct sockaddr_in address;
  int sock;
  fd_set readSet, writeSet;

  struct sockaddr_in servAddr;
  char addr[8];

  vector<string> host_list;
  struct hostent* he;
  char hostname[64];
  char hostaddr[16];

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 1000;

  // read in the IP addresses of hosts that might be running the program
  host_list = read_hosts("hosts.txt");

  // we don't want to connect to ourself, so figure out what our own IP is
  if (gethostname(hostname, 32) == -1)
  {
    perror("gethostname");
    // TODO: handle error
  }
  he = gethostbyname(hostname);
  if (he == NULL)
  {
    perror("gethostbyname");
    // TODO: handle error
  }
  strncpy(hostaddr, inet_ntoa(*((struct in_addr*)he->h_addr)), 16);

  while (1)
  {
    for (int i = 0; i < host_list.size(); i++)
    {
      // if the ip address we're looking at is NOT the local one and we aren't already
      // connected to it, try to connect to it
      if (strcmp(hostaddr, host_list[i].c_str()) != 0 && find(connected_list.begin(),
            connected_list.end(), host_list[i]) == connected_list.end())
      {
        sock = socket(AF_INET, SOCK_STREAM, 0);

        memset(&servAddr, '0', sizeof(servAddr));

        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(PORT);

        //Convert IPv4 addresses from text to binary form
        inet_pton(AF_INET, host_list[i].c_str(), &servAddr.sin_addr);

        // set the timeout on the connect() call so we don't wait forever
        if (setsockopt (sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
          perror("setsockopt");
          // TODO: handle error
        }
        // try to connect to the address
        if (connect(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
          close(sock); // if it fails, close the socket so we can reuse the number
        }
        else {
          // if it's a brand new connection, add its socket to the list of sockets the client
          // is connected to
          if (find(client_fd_list.begin(), client_fd_list.end(), sock) == client_fd_list.end()) {
            // ----------------------------------------------------------------------
            // must be in the lock because the client fd list is shared with the client requester thread
            if (pthread_mutex_lock(&client_fd_lock) == -1)
            {
              perror("pthread_mutex_lock");
              // TODO: do something here to handle the error
            }
            client_fd_list.push_back(sock);
            connected_list.push_back(host_list[i]);
            if (pthread_mutex_unlock(&client_fd_lock) == -1)
            {
              perror("pthread_mutex_unlock");
              // TODO: handle the error
            }
            // ----------------------------------------------------------------------
            printf("Successfully connected to %s\n", host_list[i].c_str());
          }
        }
      }
    }
    // sleep for a bit before we try to connect again (so we don't clog up the network too much)
    if (usleep(1000000) == -1)
    {
      perror("usleep");
      // TODO: handle error
    }
  }
}

int writeToFile(struct serverMessage* toWrite, string fileName){
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

/* -----------------------------------------------------------------------------
 * vector<string> read_hosts(string filename)
 * Given a file of known host IP addresses, reads them and puts them in a
 * vector of strings. We use this vector of strings to connect to new servers.
 * Parameters:
 * - string filename: the name of the file to read
 * Returns: a vector of strings, where each is an IP address of a host that
 * might be running the program.
 * ---------------------------------------------------------------------------*/
vector<string> read_hosts(string filename) {
  ifstream file;
  vector<string> list; // will hold the list of IP addrs

  file.open(filename);
  if (!file)
  {
    perror("open");
  }
  // read the IP addresses in from the file
  string ip;
  while (file >> ip)
  {
    list.push_back(ip);
  }

  file.close();
  return list;
}

#endif /* CLIENT_H */

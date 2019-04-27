# ifndef SERVER_H
# define SERVER_H

#include <iomanip>
#include <fstream>
#include <string>

using namespace std;

/*
Paul's notes to self:
Client needs a way to know when it is finished recieving data so it stops
requesting - solution: server needs to return file size when it sends the first
packet of the transmission - add a file size parameter to serverMessage

lock notes:
use lock around code that uses client_fd_list to avoid race conditions

broad strokes notes for 4/25:
- initial broadcast to all peers asking if they have a file
- Handing recieving different parts of a file and adding them
  - keep file open until we've recieved whole thing - add elements as we recieve
    them from the server
*/
typedef unsigned char *byte_pointer;

# define PORT 8080 // the port that the server listens for new connections on

fd_set server_readfds; // global set of file descriptors for the reading server thread to watch
pthread_mutex_t server_readfd_lock; // lock for server_readfds since it's global and accessed by multiple threads
vector<int> server_fd_list; // list of currently active file descriptors

// for signaling that there is a new variable
pthread_mutex_t signal_lock;
pthread_cond_t signal_var;
vector<string> new_connections;

void* start_server_listen(void* arg);
void* start_server_read(void* arg);
int server_listen(void);
int server_read (void);
int serverSend(char* toSend, int sendFD);
int arrayCheck(char* toCheck, int arraySize);

struct serverMessage{
  long positionInFile;  //what position the data should be placed in
  int bytesToUse;       //the number of bytes in the data that are "real" data - is typically 1024 unless a file
                        //has less than 1024 bytes of data left to send.
  long fileSize;        //file size in bytes
  char data[1024];      //actual data we are returning
  char hasFile;         //Updated if haveFile in the client message is 1 - has value 1 if we have file, 0 if not
}serverMessage;

struct clientMessage{
  char fileName[128]; //name of the file we are requesting
  long portionToReturn;//the portion of the file we want returned - an integer corresponding to the Nth kilobyte
                       //has to be a long to work correctly with seekg in readFile
  char haveFile;       //A boolean value that tells the server we are asking if they have the file in fileName
                       //when actually requesting file data this is 0, when asking if a server has a file is 1
}clientMessage;

void showBytes(byte_pointer start, size_t len){
  int i;
  for (i=0; i<len; i++)
    printf(" %.2x", start[i]);
  printf("\n");
}

int arrayCheck(char* toCheck, int arraySize){
  int sum=0;
  for (int i = 0; i<arraySize; i++){
    sum = sum+toCheck[i];
  }
  if(sum != 0){
    return 1;
  }
  return 0;
}
/* -----------------------------------------------------------------------------
 * void* start_server_listen (void* arg)
 * Function for the server listener thread to start in. Calls server_listen()
 * where the server sets up and starts listening for new connections.
 * Parameters:
 * - void* arg: a void pointer required by pthread_create(). Not used.
 * Returns: nothing
 * ---------------------------------------------------------------------------*/
void* start_server_listen(void* arg) {
  server_listen();
  pthread_exit(NULL);
}

/* -----------------------------------------------------------------------------
 * void* start_server_read (void* arg)
 * Function for the server reader thread to start in. Calls server_read()
 * where the server calls select() in a loop to read new messages coming in
 * from clients.
 * Parameters:
 * - void* arg: a void pointer required by pthread_create(). Not used.
 * Returns: nothing
 * ---------------------------------------------------------------------------*/
void* start_server_read(void* arg) {
  server_read();
  pthread_exit(NULL);
}

char* readFile(struct clientMessage* toRetrieve){
  std::ifstream inFile;
  char toSend[1024];
  size_t startLocation;
  size_t length;
  size_t size = 0;
  struct serverMessage* toReturn;
  toReturn = (struct serverMessage*)malloc(sizeof(struct serverMessage));

  inFile.open(toRetrieve->fileName);
  if (!inFile && toRetrieve->haveFile == 1){
    toReturn->positionInFile = toRetrieve->portionToReturn;
    toReturn->bytesToUse = length;
    toReturn->fileSize = (long)size;
    toReturn->hasFile = 0;
    return((char*)toReturn);
  }else if(!inFile){
    printf("Unable to open file\n");
    exit(0);
  }
  printf("file successfully opened \n");
  //get the total file size and set the position to the byte we have to read
  inFile.seekg(0, inFile.end);
  size = inFile.tellg();
  inFile.seekg(toRetrieve->portionToReturn*1024);

  //if there are less than 1024 bytes left in the file to read
  if (size-(toRetrieve->portionToReturn*1024) > sizeof(toSend)){
    length = sizeof(toSend);                              //we have a kilobyte to send, so send a full kilobyte
  }else{
    length = size-(toRetrieve->portionToReturn*1024);  //if not, just send the rest of the file
  }
  printf("file length assigned \n");
  inFile.read(toSend, length);
  printf("sending data: \n");
  showBytes((byte_pointer)toSend, (size_t)length);
  printf("\n %lui \n", sizeof(toReturn->data));
  memcpy(toReturn->data, toSend, length);
  toReturn->positionInFile = toRetrieve->portionToReturn;
  toReturn->bytesToUse = length;
  toReturn->fileSize = (long)size;
  toReturn->hasFile = 1;
  return((char*)toReturn);
}

/* -----------------------------------------------------------------------------
 * int server_listen (void)
 * The funtion that the server listener thread runs in. Sets up the socket to
 * listen for new connections and then calls accept() in a loop to accept all
 * new client connections. When a client connects, a file descriptor is assigned
 * to the connection and added to the global fd_set server_readfds that the reader
 * thread uses to receive client messages.
 * Parameters: none
 * Returns: nothing right now
 * ---------------------------------------------------------------------------*/
int server_listen(void) {
  int serverFd, newSocket;
  int opt = 1;
  struct sockaddr_in address;
  int addrlen = sizeof(address);

  // create the listening file descriptor
  // TODO: do we need to take care of error returns here? (i.e. what if socket()
  // or setsockopt() fail)
  serverFd = socket(AF_INET, SOCK_STREAM, 0);

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;

  address.sin_port = htons(PORT);

  //if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) < 0)
  if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
  {
    perror("setsockopt");
  }

  // attach socket to the port
  if (::bind(serverFd, (struct sockaddr *)&address, sizeof(address)) == -1) {
    perror("bind");
    // TODO: exit? do something
  }

  // start listening for client connections
  if (listen(serverFd, 5) == -1)
  {
    perror("listen");
  }

  // loop, listening for new conections
  while (1)
  {
    newSocket = accept(serverFd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (newSocket == -1)
    {
      perror("accept");
    }
    else {
      // ----------------------------------------------------------------------
      // get the lock and add it to the set of file descriptors to listen to
      if (pthread_mutex_lock(&server_readfd_lock) == -1)
      {
        perror("pthread_mutex_lock");
        // TODO: do something here to handle the error
      }
      FD_SET(newSocket, &server_readfds); // put the file descriptor in the set
      //readfd_count++;
      server_fd_list.push_back(newSocket);
      if (pthread_mutex_unlock(&server_readfd_lock) == -1)
      {
        perror("pthread_mutex_unlock");
        // TODO: handle the error
      }
    }

  }

  return 0;
}

/* -----------------------------------------------------------------------------
 * int server_read (void)
 * Function that the server reader thread runs. In a loop, calls select() on the
 * fd_set server_readfds of client connection file descriptors that is populated by the
 * listener function. If it receives a request from a client, determines what
 * information the client wants and sends back the correct information.
 * Parameters: none
 * Returns: nothing right now
 * ---------------------------------------------------------------------------*/
int server_read (void) {
  printf("server read\n");
  int ready;

  // initialize the other two FD sets that select() needs; they'll be empty
  fd_set writefds;
  fd_set exceptfds;

  // copy of server_readfds to pass to select(); this is necessary because select()
  // changes the fd_sets it gets IN PLACE and we need to remember what was
  // in the set
  fd_set server_readfds_copy;

  // timeout interval for the select() call. if it doesn't get anything, we
  // want it to timeout so that the listener thread can update the set of
  // file descriptors that it uses if any new connections have come in
  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 100000; // 100 milliseconds; WE MAY WANT TO CHANGE THIS
  while (1) {
    // clear out the file descriptor sets to ensure they are empty
    FD_ZERO(&server_readfds_copy);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    // ----------------------------------------------------------------------
    // get the lock so that we can make a copy of server_readfds to pass to select
    if (pthread_mutex_lock(&server_readfd_lock) == -1)
    {
      perror("pthread_mutex_lock");
      // TODO: do something here to handle the error
    }
    server_readfds_copy = server_readfds;
    int nfds = 0;
    // determine what nfds should be; it is supposed to be the largest FD in the set + 1
    // TODO: THIS IS SUPER INEFFICIENT, WE SHOULD JUST KEEP THIS VECTOR SORTED
    for (int i = 0; i < server_fd_list.size(); i++)
    {
      if (server_fd_list[i] > nfds)
      {
        nfds = server_fd_list[i];
      }
    }
    nfds++;
    if (pthread_mutex_unlock(&server_readfd_lock) == -1)
    {
      perror("pthread_mutex_unlock");
      // TODO: handle the error
    }
    // ----------------------------------------------------------------------

    // call select on the copy of server_readfds
    ready = select(nfds, &server_readfds_copy, &writefds, &exceptfds, &timeout);
    if (ready == -1) {
      perror("select");
      // TODO: handle the error
    }
    else if (ready != 0) {
      // iterate through all of the file descriptors that we may have
      // gotten data from and determine which ones actually sent us something
      // ----------------------------------------------------------------------
      // we need the lock here because we access server_fd_list, which is shared with
      // the listener thread
      if (pthread_mutex_lock(&server_readfd_lock) == -1)
      {
        perror("pthread_mutex_lock");
        // TODO: do something here to handle the error
      }
      for (int i = 0; i < server_fd_list.size(); i++)
      {
        if (FD_ISSET(server_fd_list[i], &server_readfds_copy) != 0) {
          printf("got data from fd %i\n", server_fd_list[i]);

          size_t requestSize = sizeof(clientMessage);
          char* buffer = new char[requestSize];
          memset(buffer, 0, sizeof(clientMessage));
          int valRead = read(server_fd_list[i], buffer, sizeof(clientMessage)); // weird stuff is happening here
          if (valRead > 0){

            if (arrayCheck(buffer, (int)sizeof(clientMessage))){
              struct clientMessage* toAccept = (struct clientMessage*)buffer;

              printf("printing received message:\n");
              printf("fileName requested: %s and returning starting at block %ld\n", toAccept->fileName, toAccept->portionToReturn);

              showBytes((byte_pointer)toAccept->fileName, sizeof(clientMessage));
              printf("\n");
              showBytes((byte_pointer)buffer, sizeof(clientMessage));

              char* toReturn = readFile(toAccept);
              send(server_fd_list[i], toReturn, sizeof(serverMessage), 0);
            }else{
              printf("Null message recieved\n");
            }
            free(buffer);
          }
          else{
            printf("other side is disconnected\n");
            // if we get here, the other side disconnected, so close this fd and remove it from our lists
            close(server_fd_list[i]);
            FD_CLR(server_fd_list[i], &server_readfds);
            server_fd_list.erase(server_fd_list.begin() + i);
            free(buffer);
            //perror("read");
          }

        }
      }
      // release the lock
      if (pthread_mutex_unlock(&server_readfd_lock) == -1)
      {
        perror("pthread_mutex_unlock");
        // TODO: handle the error
      }
      // ----------------------------------------------------------------------
    }

  }

}

int serverSend(char* toSend, int sendFD){
  struct sockaddr_in address;
  socklen_t peerAddrLen = sizeof(address);
//  getpeername(sendFD, &address, &peerAddrLen);
  return 1;
}
# endif /* SERVER_H */

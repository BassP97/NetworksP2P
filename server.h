# ifndef SERVER_H
# define SERVER_H

#include <iomanip>
#include <fstream>
#include <string>

# define PORT 8080 // the port that the server listens for new connections on

fd_set readfds; // global set of file descriptors for the reading server thread to watch
int readfd_count; // number of file descriptors in the set
pthread_mutex_t readfd_lock; // lock for readfds since it's global and accessed by multiple threads
std::vector<int> fd_list; // list of currently active file descriptors

void* start_server_listen(void* arg);
void* start_server_read(void* arg);
int server_listen(void);
int server_read (void);

struct fileRequest{
  char fileName[128]; //name of the file we are requesting
  long portionToReturn;//the portion of the file we want returned - an integer corresponding to the Nth kilobyte
                       //has to be a long to work correctly with seekg in readFile
}fileRequest;

struct fileReturn{
  char data[1024];      //actual data we are returning
  char positionInFile;  //what position the data should be placed in
  int bytesToUse;       //the number of bytes in the data that are "real" data - is typically 1024 unless a file
                        //has less than 1024 bytes of data left to send.
}fileReturn;

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

char* readFile(struct fileRequest* toRetrieve){
  std::string fileName = toRetrieve->fileName;
  std::ifstream inFile;
  char toSend[1024];
  size_t startLocation;
  struct fileReturn* toReturn;
  toReturn = (struct fileReturn*)malloc(sizeof(struct fileReturn));


  inFile.open("fileName");
  if (!inFile) {
      printf("Unable to open file");
      exit(0);
  }

  //get the total file size and set the position to the byte we have to read
  inFile.seekg(0, inFile.end);
  size_t length = inFile.tellg();
  inFile.seekg(toRetrieve->portionToReturn*1024);

  //if there are less than 1024 bytes left in the file to read
  if (length-(toRetrieve->portionToReturn*1024) > sizeof(toSend)){
    length = sizeof(toSend);                              //we have a kilobyte to send, so send a full kilobyte
  }else{
    length = length-(toRetrieve->portionToReturn*1024);  //if not, just send the rest of the file
  }

  inFile.read(toSend, length);
  memcpy(toReturn->data, toSend, startLocation);
  toReturn->positionInFile = toRetrieve->portionToReturn;
  toReturn->bytesToUse = length;
  return((char*)toReturn);
}

/* -----------------------------------------------------------------------------
 * int server_listen (void)
 * The funtion that the server listener thread runs in. Sets up the socket to
 * listen for new connections and then calls accept() in a loop to accept all
 * new client connections. When a client connects, a file descriptor is assigned
 * to the connection and added to the global fd_set readfds that the reader
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
  setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;

  address.sin_port = htons(PORT);

  // attach socket to the port
  if (bind(serverFd, (struct sockaddr *)&address, sizeof(address)) == -1) {
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
    // TODO: dynamic ports for each connection? Do we need that?
    newSocket = accept(serverFd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (newSocket == -1)
    {
      perror("accept");
    }

    // ----------------------------------------------------------------------
    // get the lock and add it to the set of file descriptors to listen to
    if (pthread_mutex_lock(&readfd_lock) == -1)
    {
      perror("pthread_mutex_lock");
      // TODO: do something here to handle the error
    }
    FD_SET(newSocket, &readfds); // put the file descriptor in the set
    readfd_count++;
    fd_list.push_back(newSocket);
    if (pthread_mutex_unlock(&readfd_lock) == -1)
    {
      perror("pthread_mutex_unlock");
      // TODO: handle the error
    }
    // ----------------------------------------------------------------------
  }

  return 0;
}

/* -----------------------------------------------------------------------------
 * int server_read (void)
 * Function that the server reader thread runs. In a loop, calls select() on the
 * fd_set readfds of client connection file descriptors that is populated by the
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

  // copy of readfds to pass to select(); this is necessary because select()
  // changes the fd_sets it gets IN PLACE and we need to remember what was
  // in the set
  fd_set readfds_copy;
  int readfd_count_copy;

  // timeout interval for the select() call. if it doesn't get anything, we
  // want it to timeout so that the listener thread can update the set of
  // file descriptors that it uses if any new connections have come in
  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 100000; // 100 milliseconds; WE MAY WANT TO CHANGE THIS

  while (1) {
    // clear out the file descriptor sets to ensure they are empty
    FD_ZERO(&readfds_copy);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    // ----------------------------------------------------------------------
    // get the lock so that we can make a copy of readfds to pass to select
    if (pthread_mutex_lock(&readfd_lock) == -1)
    {
      perror("pthread_mutex_lock");
      // TODO: do something here to handle the error
    }
    readfds_copy = readfds;
    readfd_count_copy = readfd_count;
    int nfds = 0;
    // determine what nfds should be; it is supposed to be the largest FD in the set + 1
    // TODO: THIS IS SUPER INEFFICIENT, WE SHOULD JUST KEEP THIS VECTOR SORTED
    for (int i = 0; i < fd_list.size(); i++)
    {
      if (fd_list[i] > nfds)
      {
        nfds = fd_list[i];
      }
    }
    nfds++;
    if (pthread_mutex_unlock(&readfd_lock) == -1)
    {
      perror("pthread_mutex_unlock");
      // TODO: handle the error
    }
    // ----------------------------------------------------------------------

    // call select on the copy of readfds
    ready = select(nfds, &readfds_copy, &writefds, &exceptfds, &timeout);
    if (ready == -1) {
      perror("select");
      // TODO: handle the error
    }
    else if (ready != 0) {
      // iterate through all of the file descriptors that we may have
      // gotten data from and determine which ones actually sent us something
      // ----------------------------------------------------------------------
      // we need the lock here because we access fd_list, which is shared with
      // the listener thread
      if (pthread_mutex_lock(&readfd_lock) == -1)
      {
        perror("pthread_mutex_lock");
        // TODO: do something here to handle the error
      }
      for (int i = 0; i < fd_list.size(); i++)
      {
        if (FD_ISSET(fd_list[i], &readfds_copy) != 0) {
          printf("got data from fd %i\n", fd_list[i]);
          // some temporary code to read the message from the client
          size_t requestSize = sizeof(fileRequest);
          char* buffer = new char[requestSize];
          memset(buffer, 0, sizeof(fileRequest));
          int valRead = read(fd_list[i], buffer, sizeof(fileRequest));
          struct fileRequest* toAccept = (struct fileRequest*)buffer;

          if (valRead > 0){
            printf("printing received message:\n");
            printf("fileName requested: %s and returning starting at block %ld\n", toAccept->fileName, toAccept->portionToReturn);
          }
          else
          {
            // TODO: IF WE EXIT THE LOOP HERE, WE WILL NEED TO RELEASE THE LOCK
            free(buffer);
            perror("read");
          }
          char* toReturn = readFile(toAccept);
          free(buffer);
        }
      }
      // release the lock
      if (pthread_mutex_unlock(&readfd_lock) == -1)
      {
        perror("pthread_mutex_unlock");
        // TODO: handle the error
      }
      // ----------------------------------------------------------------------
    }

  }

}

# endif /* SERVER_H */

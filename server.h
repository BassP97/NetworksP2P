# ifndef SERVER_H
# define SERVER_H

# define PORT 8080 // the port that the server listens for new connections on

fd_set readfds; // global set of file descriptors for the reading server thread to watch
int readfd_count; // number of file descriptors in the set
pthread_mutex_t readfd_lock; // lock for readfds since it's global and accessed by multiple threads
vector<int> fd_list; // list of currently active file descriptors

void* start_server_listen(void* arg);
void* start_server_read(void* arg);
int server_listen(void);
int server_read (void);

void* start_server_listen(void* arg) {
  server_listen();
  pthread_exit(NULL);
}

void* start_server_read(void* arg) {
  server_read();
  pthread_exit(NULL);
}

int server_listen(void) {
  printf("initialize server\n");
  int serverFd, newSocket;
  int opt = 1;
  struct sockaddr_in address;
  int addrlen = sizeof(address);

  // create the listening file descriptor
  // TODO: do we need to take care of error returns here?
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
    printf("new socket: %i\n", newSocket);

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
  timeout.tv_usec = 100000; // 100 milliseconds; WE MAY WANT TO CHANGE THIS/

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
      // TODO: SHOULD PUT THE LOCK AROUND THIS LOOP SINCE IT ACCESSES FD_LIST
      for (int i = 0; i < fd_list.size(); i++)
      {
        if (FD_ISSET(fd_list[i], &readfds_copy) != 0) {
          printf("got data from fd %i\n", fd_list[i]);
          // some temporary code to read the message from the client
          char buffer[1024];
          memset(buffer, 0, 1024);
          int valRead = read(fd_list[i], buffer, 1024);
          if (valRead > 0)
          {
            printf("printing received message:\n");
            printf("%s\n", buffer);
          }
          else
          {
            perror("read");
          }
        }
      }
    }

  }

}

# endif /* SERVER_H */

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <iostream>
#include <pthread.h>
#include <sys/select.h>
#include <vector>
#include <netdb.h>
#include <algorithm>
#include "client.h"
#include "server.h"

using namespace std;

// TODO: eventually will need a way for the user to safely shut down the whole thing on their end
// TODO: make a function to destroy resources when we have to quit due to an error
// TODO: be careful to close connections when we need to. and stuff like that.

int main(int argc,char *argv[]) {

  pthread_t threads[4]; // 3 threads
  int rc;

  // initialize lock for list of FDs to read in select()
  if (pthread_mutex_init(&server_readfd_lock, NULL) == -1)
  {
    perror("pthread_mutex_init");
    // TODO: do something here
  }

  // initialize lock for client FD
  if (pthread_mutex_init(&client_fd_lock, NULL) == -1)
  {
    perror("pthread_mutex_init");
    // TODO: do something here
  }

  // ----------------------------------------------------------------------
  // ensure that the readfds fd_set is zeroed out
  if (pthread_mutex_lock(&server_readfd_lock) == -1)
  {
    perror("pthread_mutex_lock");
    // TODO: do something here to handle the error
  }
  FD_ZERO(&server_readfds);
  if (pthread_mutex_unlock(&server_readfd_lock) == -1)
  {
    perror("pthread_mutex_unlock");
    // TODO: handle the error
  }
  // ----------------------------------------------------------------------

  // create client connector thread
  rc = pthread_create(&threads[0], NULL, start_client_connector, NULL);
  if (rc)
  {
    perror("pthread_create");
    if (pthread_mutex_destroy(&server_readfd_lock) == -1)
    {
      perror("pthread_mutex_destroy");
    }
    if (pthread_mutex_destroy(&client_fd_lock) == -1)
    {
      perror("pthread_mutex_destroy");
    }
    pthread_exit(NULL);
    exit(-1);
  }

  // create client requester thread
  rc = pthread_create(&threads[1], NULL, start_client_requester, NULL);
  if (rc)
  {
    perror("pthread_create");
    if (pthread_mutex_destroy(&server_readfd_lock) == -1)
    {
      perror("pthread_mutex_destroy");
    }
    if (pthread_mutex_destroy(&client_fd_lock) == -1)
    {
      perror("pthread_mutex_destroy");
    }
    pthread_exit(NULL);
    exit(-1);
  }

  // create server listener thread
  rc = pthread_create(&threads[2], NULL, start_server_listen, NULL);
  if (rc)
  {
    perror("pthread_create");
    if (pthread_mutex_destroy(&server_readfd_lock) == -1)
    {
      perror("pthread_mutex_destroy");
    }
    if (pthread_mutex_destroy(&client_fd_lock) == -1)
    {
      perror("pthread_mutex_destroy");
    }
    pthread_exit(NULL);
    exit(-1);
  }

  // create server reader thread
  rc = pthread_create(&threads[3], NULL, start_server_read, NULL);
  if (rc)
  {
    perror("pthread_create");
    if (pthread_mutex_destroy(&server_readfd_lock) == -1)
    {
      perror("pthread_mutex_destroy");
    }
    if (pthread_mutex_destroy(&client_fd_lock) == -1)
    {
      perror("pthread_mutex_destroy");
    }
    pthread_exit(NULL);
    exit(-1);
  }

  // destroy the lock
  if (pthread_mutex_destroy(&server_readfd_lock) == -1)
  {
    perror("pthread_mutex_destroy");
  }
  if (pthread_mutex_destroy(&client_fd_lock) == -1)
  {
    perror("pthread_mutex_destroy");
  }
  pthread_exit(NULL);

  return 0;
}

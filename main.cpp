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
#include <fcntl.h>
#include "client.h"
#include "server.h"

using namespace std;

int main(int argc,char *argv[]) {

  pthread_t threads[4]; // 4 threads plus this one
  int rc;

  // initialize lock for list of FDs to read in select()
  if (pthread_mutex_init(&server_readfd_lock, NULL) == -1)
  {
    perror("pthread_mutex_init");
    return 1;
  }

  // initialize lock for client FD
  if (pthread_mutex_init(&client_fd_lock, NULL) == -1)
  {
    perror("pthread_mutex_init");
    pthread_mutex_destroy(&server_readfd_lock);
    return 1;
  }

  // initialize stop lock
  if (pthread_mutex_init(&stop_lock, NULL) == -1)
  {
    perror("pthread_mutex_init");
    pthread_mutex_destroy(&server_readfd_lock);
    pthread_mutex_destroy(&client_fd_lock);
    return 1;
  }

  // ----------------------------------------------------------------------
  // ensure that the readfds fd_set is zeroed out
  if (pthread_mutex_lock(&server_readfd_lock) == -1)
  {
    perror("pthread_mutex_lock");
    pthread_mutex_destroy(&server_readfd_lock);
    pthread_mutex_destroy(&client_fd_lock);
    pthread_mutex_destroy(&stop_lock);
    return 1;
  }
  FD_ZERO(&server_readfds);
  if (pthread_mutex_unlock(&server_readfd_lock) == -1)
  {
    perror("pthread_mutex_unlock");
    pthread_mutex_destroy(&server_readfd_lock);
    pthread_mutex_destroy(&client_fd_lock);
    pthread_mutex_destroy(&stop_lock);
    return 1;
  }
  // ----------------------------------------------------------------------

  // create client requester thread
  rc = pthread_create(&threads[0], NULL, start_client_requester, NULL);
  if (rc)
  {
    perror("pthread_create");
    pthread_mutex_destroy(&server_readfd_lock);
    pthread_mutex_destroy(&client_fd_lock);
    pthread_mutex_destroy(&stop_lock);
    return 1;
  }

  // create client connector thread
  rc = pthread_create(&threads[1], NULL, start_client_connector, NULL);
  if (rc)
  {
    perror("pthread_create");
    pthread_mutex_destroy(&server_readfd_lock);
    pthread_mutex_destroy(&client_fd_lock);
    pthread_mutex_destroy(&stop_lock);
    return 1;
  }

  // create server listener thread
  rc = pthread_create(&threads[2], NULL, start_server_listen, NULL);
  if (rc)
  {
    perror("pthread_create");
    pthread_mutex_destroy(&server_readfd_lock);
    pthread_mutex_destroy(&client_fd_lock);
    pthread_mutex_destroy(&stop_lock);
    return 1;
  }

  // create server reader thread
  rc = pthread_create(&threads[3], NULL, start_server_read, NULL);
  if (rc)
  {
    perror("pthread_create");
    pthread_mutex_destroy(&server_readfd_lock);
    pthread_mutex_destroy(&client_fd_lock);
    pthread_mutex_destroy(&stop_lock);
    return 1;
  }

  // join on the threads to wait for them to finish
  for (int i = 0; i < 4; i++)
  {
    pthread_join(threads[i], NULL);
  }

  // destroy the locks and exit once all threads have finished
  if (pthread_mutex_destroy(&server_readfd_lock) == -1)
  {
    perror("pthread_mutex_destroy");
    pthread_mutex_destroy(&client_fd_lock);
    pthread_mutex_destroy(&stop_lock);
    return 1;
  }
  if (pthread_mutex_destroy(&client_fd_lock) == -1)
  {
    perror("pthread_mutex_destroy");
    pthread_mutex_destroy(&stop_lock);
    return 1;
  }
  if (pthread_mutex_destroy(&stop_lock) == -1)
  {
    perror("pthread_mutex_destroy");
    return 1;
  }

  return 0;
}

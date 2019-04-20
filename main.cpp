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
#include "client.h"
#include "server.h"

using namespace std;

int main(int argc,char *argv[]) {

  pthread_t threads[2]; // 2 threads - one client, one server
  int rc;

  rc = pthread_create(&threads[0], NULL, start_client, NULL);
  if (rc)
  {
    perror("pthread_create");
    exit(-1);
  }

  rc = pthread_create(&threads[0], NULL, start_server, NULL);
  if (rc)
  {
    perror("pthread_create");
    exit(-1);
  }

  pthread_exit(NULL);
  
  return 0;
}

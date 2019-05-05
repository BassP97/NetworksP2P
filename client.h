#ifndef CLIENT_H
#define CLIENT_H

#include <map>
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
void stop_client_requester(char* fileNameArr);

vector<int> whoHasFile(char* message, long& fileSize, char* rawBuffer);
int sendRequest(struct clientMessage &toRequest, int &filePosition, char* message);
int reInitialiseFDSet(vector<int> &serversWithFile, fd_set &readFDSet);
int readData(long fileSize, map<int,bool> &portionCheck, string fileName, long & bytesReceived, vector<int> &serversWithFile, fd_set readFDSet, char* rawBuffer);
int resendData(map<int,bool>& portionCheck, map<int,bool>::iterator it, char* fileNameArr, char* message, vector<int>& serversWithFile);

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

/* -----------------------------------------------------------------------------
 * void stop_client_requester (void)
 * Called when a fatal error occurs in the client requester thread. Sets the
 * stopped variable to 1 to let other threads know they should exit. This thread
 * does not have any resources to free on exit. Does NOT return or exit the thread;
 * the caller is responsible for doing that after calling stop_client_requester();
 * Parameters: none
 * Returns: none
 * ---------------------------------------------------------------------------*/
void stop_client_requester(char* fileNameArr)
{
  if (pthread_mutex_lock(&stop_lock) == -1)
  {
    perror("pthread_mutex_lock");
  }
  stopped = 1;
  if (pthread_mutex_unlock(&stop_lock) == -1)
  {
    perror("pthread_mutex_lock");
  }
  delete[] fileNameArr;
}
/* -----------------------------------------------------------------------------
 * int sendRequest(int sendLocation, struct clientMessage &toRequest,
 *  int &filePosition, char* message)
 * A function that sends a given clientMessage to a destination specified by
 * a file descriptor sendLocation.
 * Parameters:
 * - int sendLocation: the socket file descriptor to send the message to
 * - struct &clientMessage &toRequest: a clientMessage that contains the
 *   information to send. Passed by reference.
 * - int &filePosition: the current filePosition at the beginning of the loop
 *   that sendRequest is called in. Put into toRequest and incremented. Passed
 *   by reference.
 * - char* message: the buffer to send. Passed by reference. In sendRequest,
 *   we cast toRequest to a character pointer and set message equal to it.
 * Returns: 1 if the send was successful, -1 otherwise.
 * ---------------------------------------------------------------------------*/
int sendRequest(int sendLocation, struct clientMessage &toRequest, int &filePosition, char* message) {
  toRequest.portionToReturn = filePosition;
  filePosition++;

  //cast our message to a pointer
  message = (char*)&toRequest;

  // printf("SENDING %i to %i\n", toRequest.portionToReturn, serversWithFile[i]);
  // sent does NOT tell us if anything failed
  if (send(sendLocation, message, sizeof(clientMessage), 0) == -1) {
    perror("send");
    return -1;
  }
  return 1;
}

/* -----------------------------------------------------------------------------
 * vector<int> whoHasFile(char* message, long& fileSize, char* rawBuffer)
 * A function that determiners which of the connected servers has the requested
 * file.
 * Parameters:
 * - char* message: the file existence query message to send to the servers.
 * - long &fileSize: the size of the requested file. Initially this is
 *   unknown, but we fill it in once we get a positive file existence response
 *   from one of the servers. Passed by reference.
 * - char* rawBuffer: a buffer to read file existence queries into.
 * Returns: a vector of integers containing the file descriptors of the sockets
 * associated with the connections with servers that have the requested file.
 * ---------------------------------------------------------------------------*/
vector<int> whoHasFile(char* message, long& fileSize, char* rawBuffer){
  vector<int> fileList;
  vector<int> failVec;
  struct serverMessage* serverReturn;
  int sent;
  int valRead;
  if (pthread_mutex_lock(&client_fd_lock) == -1){
    perror("pthread_mutex_lock");
    return failVec;
  }

  //We iterate through all current active connections and send them a message
  //asking if they have a file - if yes we append them to our "servers with file"
  //vector. This is also where we get the total file size of the file we are requesting
  for(int i = 0; i<client_fd_list.size(); i++){
    sent = send(client_fd_list[i], message, sizeof(struct clientMessage), 0);
    if (sent == -1) {
      perror("send");
      return failVec;
    }

    valRead = read(client_fd_list[i], rawBuffer, sizeof(struct serverMessage));
    if (valRead == -1){
      perror("read");
      return failVec;
    }

    serverReturn = (struct serverMessage*)rawBuffer;
    fileSize = serverReturn->fileSize;
    if (serverReturn->hasFile == 1){
      fileList.push_back(client_fd_list[i]);
    }
  }

  if (pthread_mutex_unlock(&client_fd_lock) == -1){
    perror("pthread_mutex_unlock");
    return failVec;
  }
  if (fileList.size()==0){
    return failVec;
  }
  return fileList;
}
/* -----------------------------------------------------------------------------
 * int reInitialiseFDSet(vector<int> &serversWithFile, fd_set &readFDSet)
 * A function that resets the fd_set structure used in select() in the client
 * requester thread. This structure needs to be reset after each call to
 * select() because select changes the passed-in fd_set structures in place,
 * and we want to check all open sockets each time we call select(). Zeroes
 * out the fd_set and sets all of the necessary file descriptors in it.
 * Parameters:
 * - vector<int> &serversWithFile: the vector of file descriptors of sockets
 *   associated with connections with servers that have the file.
 * - fd_set &readFDSet: the set of file descriptors to reset according to
 *   serversWithFile.
 * Returns: the value of the largest file descriptor in serversWithFile
 * ---------------------------------------------------------------------------*/
int reInitialiseFDSet(vector<int> &serversWithFile, fd_set &readFDSet) {
  FD_ZERO(&readFDSet);
  int largestFD = 1;
  for(int j = 0; j < serversWithFile.size(); j++){
    FD_SET(serversWithFile[j], &readFDSet);
    if(serversWithFile[j]>largestFD){
      largestFD = serversWithFile[j];
    }
  }
  largestFD++;
  return largestFD;
}

/* -----------------------------------------------------------------------------
 * int readData(long fileSize, map<int,bool> &portionCheck, string fileName,
 *  long & bytesReceived, vector<int> &serversWithFile, fd_set readFDSet,
 *  char* rawBuffer)
 * A function that called after the client requester thread calls select()
 * to sort through the sockets that have data to read. Takes care of reading
 * these messages and writing them to the correct file if they are responses
 * to data requests. Also takes care of removing disconnected sockets from
 * all necessary structures if select() indicates that one of the servers has
 * disconnected.
 * Parameters:
 * - long fileSize: the size of the file we are requesting
 * - map<int, bool> &portionCheck: a map of integers and booleans that
 *   is used to keep track of which chunks of the file we have requested and
 *   which we have actually received a response for. Passed by reference.
 * - string fileName: the name of the file we are requesting. Allows us to open
 *   the file and write data to it.
 * - long &bytesReceived: a value that keeps track of how many total bytes
 *   have been received from this file from all servers. Passed by reference
 *   so we can update it each time we get a data response.
 * - vector<int> &serversWithFile: a list of the socket file descriptors of the
 *   servers that are known to have the file. Passed by reference so that we
 *   can remove file descriptors of servers that disconnect.
 * - fd_set readFDSet: the fd_set we called select() on before calling readData
 * - char* rawBuffer: a buffer to read data into.
 * Returns: -1 if any errors occur, the current value of bytesReceived
 * otherwise.
 * ---------------------------------------------------------------------------*/
int readData(long fileSize, map<int,bool> &portionCheck, string fileName, long & bytesReceived, vector<int> &serversWithFile, fd_set readFDSet, char* rawBuffer) {
  int valRead;
  struct serverMessage* serverReturn;
  for (int j = 0; j < serversWithFile.size(); j++) {
    if (FD_ISSET(serversWithFile[j], &readFDSet)) {
      valRead = recv(serversWithFile[j], rawBuffer, sizeof(serverMessage), 0);
      if (valRead == -1){
        perror("recv");
        return -1;
      }
      if (valRead == 0) {
        int socket = serversWithFile[j];

        // remove the file descriptor from the list of servers that have the file.
        serversWithFile.erase(serversWithFile.begin()+j);

        // determine the IP address of the disconnected host and remove it from the list of
        // connected hosts - this must be done in a lock because connected_list is shared
        // with the connector thread
        struct sockaddr addr;
        socklen_t len = sizeof(struct sockaddr);
        getpeername(socket, &addr, &len);
        struct sockaddr_in* addr_in = (struct sockaddr_in*)&addr;
        // need to calloc here otherwise it doesn't update the ip address properly
        char* ip_cstr = (char*)calloc(INET_ADDRSTRLEN, sizeof(char));
        strcpy(ip_cstr, inet_ntoa(addr_in->sin_addr));
        string ipstr = ip_cstr;
        free(ip_cstr);

        // ----------------------------------------------------------------------
        // inside the lock, remove the IP address from the connected list and
        // also remove the socket from the client fd list - both are shared with
        // the client connector thread
        if (pthread_mutex_lock(&client_fd_lock) == -1)
        {
          perror("pthread_mutex_lock");
          return -1;
        }
        // search the list of connected IP addresses and remove this one
        // once we find it
        for (int k = 0; k < connected_list.size(); k++)
        {
          if (ipstr.compare(connected_list[k]) == 0)
          {
            printf("%s disconnected\n", ipstr.c_str());
            connected_list.erase(connected_list.begin()+k);
            break;
          }
        }
        client_fd_list.erase(remove(client_fd_list.begin(), client_fd_list.end(), socket), client_fd_list.end());
        if (pthread_mutex_unlock(&client_fd_lock) == -1)
        {
          perror("pthread_mutex_unlock");
          return -1;
        }
        // ----------------------------------------------------------------------
        // finally, close the socket so that it can be reused
        close(socket);
        // break out of the for loop to ensure that we don't have any issues
        // by continuing to use a for loop with the serversWithFile vector altered
        break;
      }
      serverReturn = (struct serverMessage*)rawBuffer;

      //If we have already recieved the message (ie portioncheck contains true for this message)
      //then ignore it. Else, make sure the outOfRange flag isn't set and write it to the file
      if(serverReturn->outOfRange == 0 && !portionCheck[serverReturn->positionInFile]){
        portionCheck[serverReturn->positionInFile] = true;
        bytesReceived += serverReturn->bytesToUse;
        if(!writeToFile(serverReturn, fileName)) {
          printf("failed to write to file\n");
        }
      }
      else if (serverReturn->outOfRange == 1){
        bytesReceived = fileSize+1; // break out
        break;
      }
      // when we have received the whole file, break out of the loop
      if (bytesReceived >= fileSize) {
        break;
      }
    }
  }
  return bytesReceived;
}

/* -----------------------------------------------------------------------------
 * int resendData(long& fileSize, string fileName, long &bytesReceived,
 *  map<int,bool>& portionCheck, map<int,bool>::iterator it, char* fileNameArr,
 *  char* message, vector<int>& serversWithFile)
 * A function that is called when the client requester thread notices that we
 * are missing a message that should have come in a while ago. Messages go
 * missing in cases where a server drops out of the connection in such a way
 * that the requester doesn't immediately start requesting its missing
 * messages from the other servers. Forms a clientMessage with the correct
 * fields and sends it to a server, then receives and processes that server's
 * response. NOT fault tolerant in the same way that standard file requests
 * are.
 * Parameters:
 * - long &fileSize: the number of bytes in the file being requested
 * - string fileName: the name of the file being requested
 * - long &bytesReceived: the number of bytes received from this file so far.
 *   Passed by reference so that it can be updated as we receive more messages
 * - map<int, bool> &portionCheck: a map of integers and booleans that
 *   is used to keep track of which chunks of the file we have requested and
 *   which we have actually received a response for. Passed by reference.
 * - map<int,bool>::iterator it: an iterator being used to
 *   iterate through portionCheck in the caller. When called, points to a
 *   specific key-value pair in the map that indicates which chunk we want
 *   to re-request.
 * - char* fileNameArr: the cstring version of the name of the file we are
 *   requesting
 * - char* message: a character array used as a buffer to send to the
 *   destination server
 * - vector<int> &serversWithFile: a list of the socket file descriptors of
 *   servers that have the file
 * Returns: Greater than 1 if no fatal errors have occurred, less than 1
 * otherwise.
 * ---------------------------------------------------------------------------*/
int resendData(long& fileSize, string fileName, long &bytesReceived, map<int,bool>& portionCheck, map<int,bool>::iterator it,
    char* fileNameArr, char* message, vector<int>& serversWithFile) {

  int valRead;
  char rawBuffer[sizeof(serverMessage)];
  struct serverMessage* serverReturn;

  // resend the message
  struct clientMessage toRequest;
  strcpy(toRequest.fileName, fileNameArr);
  toRequest.portionToReturn = it->first;
  toRequest.haveFile = 0;
  message = (char*)&toRequest;

  int sent = send(serversWithFile[0], message, sizeof(clientMessage), 0);
  valRead = recv(serversWithFile[0], rawBuffer, sizeof(serverMessage), 0);

  serverReturn = (struct serverMessage*)rawBuffer;
  if(serverReturn->outOfRange == 0 && !portionCheck[serverReturn->positionInFile]) {
    portionCheck[serverReturn->positionInFile] = true;
    bytesReceived += serverReturn->bytesToUse;
    if(writeToFile(serverReturn, fileName) == -1){
      return -1;
    }
  } else if (serverReturn->outOfRange == 1) {
    bytesReceived = fileSize+1; // break out
    return 2;
  }
  return 1;
}

/* -----------------------------------------------------------------------------
 * void stop_client_connector (void)
 * Called when a fatal error occurs in the client_connector thread. Frees
 * resources and sets the stopped variable to 1 to let other threads know they
 * should exit. Does NOT return or exit the thread; the caller is responsible
 * for doing that after calling stop_client_requester();
 * Parameters: none
 * Returns: none
 * ---------------------------------------------------------------------------*/
void stop_client_connector(void)
{
  // ----------------------------------------------------------------------
  if (pthread_mutex_lock(&stop_lock) == -1)
  {
    perror("pthread_mutex_lock");
  }
  stopped = 1;
  if (pthread_mutex_unlock(&stop_lock) == -1)
  {
    perror("pthread_mutex_lock");
  }
  // ----------------------------------------------------------------------
  // free the client's sockets
  if (pthread_mutex_lock(&client_fd_lock) == -1)
  {
    perror("pthread_mutex_lock");
  }
  for (int i = 0; i < client_fd_list.size(); i++)
  {
    close(client_fd_list[i]);
  }
  if (pthread_mutex_unlock(&client_fd_lock) == -1)
  {
    perror("pthread_mutex_unlock");
  }
  // ----------------------------------------------------------------------
}

/* -----------------------------------------------------------------------------
 * int client_requester (void)
 * The main function that runs the client requester thread. Iterates infinitely
 * (until the user types in "quit" or another thread quits due to an error),
 * waiting for the user to ask for a file and then requesting this file from
 * the servers that have it.
 * Parameters: none
 * Returns: 0 if it exits due to a user request to quit and 1 if it quits
 * due to an error of its own or an error in another thread.
 * ---------------------------------------------------------------------------*/
int client_requester (void) {
  string fileName;
  char* message;
  int sock = 0, valRead;
  //Raw buffer holds the raw bytes that recv gives us later on in the program
  char rawBuffer[sizeof(serverMessage)];
  //Raw buffer will be cast directly to server return, making it easier for us to parse
  struct serverMessage* serverReturn;

  while (1)
  {
    // request a file from the server
    printf("Type in a filename to request from the server, or type quit to exit\n");
    cin >> fileName;

    // if the user types quit, we need to tell all of the other threads to exit
    if (fileName.compare("quit") == 0) {
      // ----------------------------------------------------------------------
      // check if the user has requested to quit the program; if they have,
      // end this thread.
      if (pthread_mutex_lock(&stop_lock) == -1) {
        perror("pthread_mutex_lock");
      }
      stopped = 1;
      if (pthread_mutex_unlock(&stop_lock) == -1) {
        perror("pthread_mutex_lock");
      }
      return 0;
      // ----------------------------------------------------------------------
    }
    //convert the file name to a cstring
    char* fileNameArr = new char[fileName.length()+1];
    strcpy(fileNameArr, fileName.c_str());

    // create a request
    struct clientMessage toRequest;
    strcpy(toRequest.fileName, fileNameArr);
    toRequest.portionToReturn = 0;
    toRequest.haveFile = 1;
    message = (char*)&toRequest;

    //============================STAGE ONE==================================
    //Identify which servers have the file we want
    int sent;
    long fileSize = 0;
    vector<int> serversWithFile = whoHasFile(message, fileSize, rawBuffer);

    //============================STAGE TWO==================================
    //Iterate through the list of servers with the file, getting the file bit by
    //bit from each of them in turn

    //Ensure that the raw buffer is empty in case we get <1kb of information
    memset(rawBuffer, 0, sizeof(serverMessage));

    //initial variable set up
    int filePortion = 0;
    toRequest.portionToReturn = 0;
    toRequest.haveFile = 0;
    int filePosition = 0;
    long bytesReceived = 0;

    //check if any servers have our file - if not don't even bother sending more messages
    if (serversWithFile.size()==0) {
      printf("No servers have the file you requested. Double check for typos!\n");
    } else {
      fd_set readFDSet;

      //This map tracks if we have recieved a response for some portion of the file
      //The key is an integer (from 1 to n where N is the final Nth kilobyte of the file)
      //while the value is a boolean that tracks whether or not we have recieved that kilobyte
      //True if we have, false if we haven't
      std::map<int,bool> portionCheck;
      int selectVal;
      struct timeval timeoutPeriod;
      struct sockaddr_in timeoutAddr;
      filePosition = 0;
      socklen_t timeoutAddrSize = sizeof(struct sockaddr_in);
      int res;

      timeoutPeriod.tv_sec = 1;
      timeoutPeriod.tv_usec = 0;

      printf("Downloading your file!\n");

      while (fileSize > bytesReceived) {
        if (fileSize > 1023)
        {
          if (filePosition % (((fileSize/1024)/10)) == 0)
          {
            printf(".\n");
          }
        }
        // ----------------------------------------------------------------------
        // check if any other threads have stopped; if they have, we need to
        // stop too
        if (pthread_mutex_lock(&stop_lock) == -1) {
          perror("pthread_mutex_lock");
        }
        if (stopped == 1) {
          if (pthread_mutex_unlock(&stop_lock) == -1) {
            perror("pthread_mutex_lock");
          }
          return 1;
        }
        if (pthread_mutex_unlock(&stop_lock) == -1) {
          perror("pthread_mutex_lock");
        }
        // ----------------------------------------------------------------------
        if (serversWithFile.size() == 0)
        {
          printf("All servers with your file have disconnected! Please try again.\n");
          if (remove(fileNameArr) < 0)
          {
            perror("remove");
            stop_client_requester(fileNameArr);
            return -1;
          }
          break;
        }

        //Send requests to every server with the file - we do this in "rounds" where
        //every server with the file gets one request per round
        for (int i = 0; i < serversWithFile.size(); i++){
          //if there isn't any more to read, then simply stop sending requests
          if (filePosition*1024 > fileSize){
            break;
          }
          if (sendRequest(serversWithFile[i], toRequest, filePosition, message)==-1){
            stop_client_requester(fileNameArr);
            return -1;
          }
          portionCheck[filePosition] = false;
        }

        //Get the replies
        int largestFD;
        for (int i = 0; i < serversWithFile.size(); i++) {
          //have to reinitialize because select alters the FD set
          largestFD = reInitialiseFDSet(serversWithFile, readFDSet);
          selectVal = select(largestFD, &readFDSet, NULL, NULL, &timeoutPeriod);

          //if select returns 0 then we have timed out
          if (selectVal == 0) {
            //handle timeouts here - do we actually have to do anything?
          } else {
            //if we haven't time out, figure out which connection has data and proceed to read from it
            //We determine which connection has data by checking which file descriptor is currently set
            bytesReceived = readData(fileSize, portionCheck, fileName, bytesReceived, serversWithFile, readFDSet, rawBuffer);
            if (bytesReceived == -1)
            {
              stop_client_requester(fileNameArr);
              return 1;
            }
          }
          // when we have received the whole file, break out of the loop
          if (bytesReceived >= fileSize) {
            break;
          }
        }

        int resendVal;
        for (map<int,bool>::iterator it=portionCheck.begin(); it!=portionCheck.end(); it++)
        {
          if (it->first < (filePosition-serversWithFile.size()) && it->second == false){
            if ((resendVal = resendData(fileSize, fileName, bytesReceived, portionCheck, it, fileNameArr, message, serversWithFile)) == 2){
              break;
            }else if (resendVal == -1){
              stop_client_requester(fileNameArr);
              return -1;
            }
          }
        }
      }
      printf("Done receiving your file!\n");
    }
    delete[] fileNameArr;
  }
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
 * Returns: 0 if it quits due to an error in another thread and 1 if it
 * quits due to its own error.
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
  timeout.tv_usec = 20000;

  // read in the IP addresses of hosts that might be running the program
  host_list = read_hosts("hosts.txt");

  // we don't want to connect to ourself, so figure out what our own IP is
  if (gethostname(hostname, 32) == -1)
  {
    perror("gethostname");
    stop_client_connector();
    return 1;
  }
  he = gethostbyname(hostname);
  if (he == NULL)
  {
    perror("gethostbyname");
    stop_client_connector();
    return 1;
  }
  strncpy(hostaddr, inet_ntoa(*((struct in_addr*)he->h_addr)), 16);
  while (1)
  {
    // ----------------------------------------------------------------------
    // check if the user has requested to quit the program; if they have,
    // end this thread.
    if (pthread_mutex_lock(&stop_lock) == -1)
    {
      perror("pthread_mutex_lock");
      stop_client_connector();
      return 1;
    }
    if (stopped == 1)
    {
      if (pthread_mutex_unlock(&stop_lock) == -1)
      {
        perror("pthread_mutex_lock");
      }
      // ----------------------------------------------------------------------
      // free the client's sockets
      if (pthread_mutex_lock(&client_fd_lock) == -1)
      {
        perror("pthread_mutex_lock");
        return 1;
      }
      for (int i = 0; i < client_fd_list.size(); i++)
      {
        close(client_fd_list[i]);
      }
      if (pthread_mutex_unlock(&client_fd_lock) == -1)
      {
        perror("pthread_mutex_unlock");
        return 1;
      }
      // ----------------------------------------------------------------------
      return 0;
    }

    if (pthread_mutex_unlock(&stop_lock) == -1)
    {
      perror("pthread_mutex_lock");
      stop_client_connector();
      return 1;
    }
    // ----------------------------------------------------------------------

    for (int i = 0; i < host_list.size(); i++)
    {
      // if the ip address we're looking at is NOT the local one and we aren't already
      // connected to it, try to connect to it
      // ----------------------------------------------------------------------
      // connected_list is shared, so we need to acquire the lock
      if (pthread_mutex_lock(&client_fd_lock) == -1)
      {
        perror("pthread_mutex_lock");
        stop_client_connector();
        return 1;
      }
      if (strcmp(hostaddr, host_list[i].c_str()) != 0 && find(connected_list.begin(),
            connected_list.end(), host_list[i]) == connected_list.end())
      {
        if (pthread_mutex_unlock(&client_fd_lock) == -1)
        {
          perror("pthread_mutex_unlock");
          stop_client_connector();
          return 1;
        }
        // ----------------------------------------------------------------------
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
          perror("socket");
        }

        memset(&servAddr, '0', sizeof(servAddr));

        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(PORT);
        //Convert IPv4 addresses from text to binary form
        inet_pton(AF_INET, host_list[i].c_str(), &servAddr.sin_addr);

        // set the socket to nonblocking
        long flags;
        if (flags = fcntl(sock, F_GETFL, NULL) < 0)
        {
          perror("fcntl");
          stop_client_connector();
          return 1;
        }
        flags = flags | O_NONBLOCK;
        if (fcntl(sock, F_SETFL, flags) < 0)
        {
          perror("fcntl");
          stop_client_connector();
          return 1;
        }

        int sock_closed = 0;

        // attempt to connect
        int ret = connect(sock, (struct sockaddr *)&servAddr, sizeof(servAddr));
        // if the connection failed
        if (ret < 0)
        {
          if (errno == EINPROGRESS)
          {
            fd_set temp_set;
            FD_ZERO(&temp_set);
            FD_SET(sock, &temp_set);
            // use select to wait until we the other host tells us it's connected, refuses
            // our connection, or times out
            if (select(sock+1, NULL, &temp_set, NULL, &timeout) > 0)
            {
              // if select DOESN't time out
              socklen_t len = sizeof(int);
              int err_val;
              // check the errors associated with the socket
              getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&err_val), &len);
              // if there are any errors, the connection didn't go through
              if (err_val)
              {
                close(sock);
                sock_closed = 1;
              }
              // otherwise, we are connected!
              else
              {
                printf("Successfully connected to %s\n", host_list[i].c_str());
                if (find(client_fd_list.begin(), client_fd_list.end(), sock) == client_fd_list.end()) {
                  // ----------------------------------------------------------------------
                  // must be in the lock because the client fd list is shared with the client requester thread
                  if (pthread_mutex_lock(&client_fd_lock) == -1)
                  {
                    perror("pthread_mutex_lock");
                    stop_client_connector();
                    return 1;
                  }
                  client_fd_list.push_back(sock);
                  connected_list.push_back(host_list[i]);
                  if (pthread_mutex_unlock(&client_fd_lock) == -1)
                  {
                    perror("pthread_mutex_unlock");
                    stop_client_connector();
                    return 1;
                  }
                  // ----------------------------------------------------------------------

                }
              }
            }
            // otherwise, select timed out or had another error; just close the socket and continue
            else
            {
              close(sock);
              sock_closed = 1;
            }
          }
          // otherwise, the connect call had some other kind of error
          else
          {
            perror("connect");
            close(sock);
            sock_closed = 1;
          }
        }
        // otherwise, we managed to connect without using select
        else
        {
          printf("Successfully connected to %s\n", host_list[i].c_str());
          if (find(client_fd_list.begin(), client_fd_list.end(), sock) == client_fd_list.end()) {
            // ----------------------------------------------------------------------
            // must be in the lock because the client fd list is shared with the client requester thread
            if (pthread_mutex_lock(&client_fd_lock) == -1)
            {
              perror("pthread_mutex_lock");
              stop_client_connector();
              return 1;
            }
            client_fd_list.push_back(sock);
            connected_list.push_back(host_list[i]);
            if (pthread_mutex_unlock(&client_fd_lock) == -1)
            {
              perror("pthread_mutex_unlock");
              stop_client_connector();
              return 1;
            }
            // ----------------------------------------------------------------------

          }
        }
        // else, it connected just fine
        // set back to nonblocking
        if (sock_closed == 0)
        {
          flags = flags & ~O_NONBLOCK;
          if (fcntl(sock, F_SETFL, flags) < 0)
          {
            perror("fcntl");
            stop_client_connector();
            return 1;
          }
        }
      }
      else
      {
        // we acquired the lock right before the if statement, so release it now
        if (pthread_mutex_unlock(&client_fd_lock) == -1)
        {
          perror("pthread_mutex_unlock");
          stop_client_connector();
          return 1;
        }
        // ----------------------------------------------------------------------
      }

    }
  }
}


/* -----------------------------------------------------------------------------
 * int writeToFile(struct serverMessage* toWrite, string fileName)
 * A function that actually writes a received file data response to the its
 * destination file.
 * Parameters:
 * - struct serverMessage* toWrite: the received serverMessage whose data
 *   should be written to the file
 * - string fileName: the name of the file to write the message to
 * Returns: -1 if an error occurs when writing to the file, 1 otherwise.
 * ---------------------------------------------------------------------------*/
int writeToFile(struct serverMessage* toWrite, string fileName) {
  //File does not exist
  if(access( fileName.c_str(), F_OK ) == -1){
    //not fstream because we only need to write - if there are errors maybe change?
    ofstream writeFile(fileName, ofstream::out | ofstream::binary);
    char *writeVal;
    writeVal = (char*)calloc(toWrite->fileSize, sizeof(char));
    // write a bunch of 0's to the file to make it big enough
    if(!writeFile.write(writeVal, toWrite->fileSize)){
      writeFile.close();
      return -1;
    }
    // write the actual data to the file
    writeFile.seekp(toWrite->positionInFile*1024, ofstream::beg);
    if(writeFile.write(toWrite->data, toWrite->bytesToUse)) {
      writeFile.close();
      return 1;
    } else {
      writeFile.close();
      return -1;
    }
  }
  //File does exist
  else {
    fstream writeFile;
    writeFile.open(fileName, ios::out | ios::in | ios::binary);
    writeFile.seekp(toWrite->positionInFile*1024, ofstream::beg);

    // showBytes((byte_pointer)toWrite->data, (size_t)toWrite->bytesToUse);

    long pos = writeFile.tellp();
    if(writeFile.write(toWrite->data, toWrite->bytesToUse)) {
      writeFile.close();
      return 1;
    } else {
      writeFile.close();
      return -1;
    }
  }
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

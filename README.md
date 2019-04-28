# NetworksP2P

A peer-to-peer file-sharing service made for our Computer Networking final project.

Compiles with included Makefile - just use `make`. The server listens to port 8080.

## Peer-to-peer structure

This peer-to-peer file-sharing service is designed for use amongst a relatively small set of distinct hosts that are all known to each other. Hypothetically, the number of hosts in participating in the service is limited only by the number of sockets allowed to a process, since each client is connected to every other server running the program. A client can only request files from a set of hosts whose IP addresses are known before starting the program. Since the service does not restrict which files a client can request, this is a necessary security feature to prevent a malicious client from taking sensitive information from arbitrary servers. 

## Concurrency

Each instance of our peer-to-peer file-sharing service running on a single host has four threads, two client threads and two server threads.

#### Client threads

There are two client threads: a connector thread and a requester thread. The division of the client into a connector thread and a requester thread allows the client to connect to all available servers and simultaneously make requests to all of them.

The connector thread repeated loops over a list of known hosts that may be running the program, attempting to establish connections with their servers. When a connection is made, the connector thread saves the socket file descriptor associated with the connection in `client_fd_list`, a shared vector with the requester thread. The connector thread continues to loop even after all active hosts have established connections because 1) more hosts may join the network at any time and 2) a host may drop out of the network and come back at any time, and we want the connector thread to be able to re-establish a connection.

The requester thread waits for the human user to input a file to request from the other servers. It then determines which of the other servers have the file and sends requests for chunks of the file to the other servers until it has received the entire file. The user can then request more files.

#### Server threads

There are two server threads: a listener thread and a reader thread. The division of the server into a listener and a reader thread allows the server to service requests from multiple different clients.

The listener thread sets up a listening port (port 8080) and just waits for clients to make connections. When a new connection comes in, the listener thread accepts it and puts the socket file descriptor for it in an `fd_set` `server_read_fds` and a vector `server_fd_list`. It then returns to listening for a new connection.

The reader thread uses the `server_read_fds` `fd_set` to repeatedly call `select()` on all of the server's open connections. When it receives a request from a client, it determines what part of what file is being requested and sends it back to the requesting client.

## Protocol

Our file-sharing service uses two main message structures and, within each, several types of messages. Clients send messages with the `clientMessage` structure and servers send messages with the `serverMessage` structure. Since clients never communicate with other clients and servers never communicate with other servers, clients can always expect to receive information in a `serverMessage` format and servers can always expect to receive information in a `clientMessage` format. We describe the structure of the client and server messages, as well as the types of information they each carry, below.

#### Client messages

A `clientMessage` struct has the following fields.

- `char fileName[128]`: specifies the name of the file that the client is requesting from the server. Always used, because a client message is always a file request or a query to determine if a specific file exists on a server.
- `long portionToReturn`: indicates which part of the file we would like from the server in a file request. Should be an integer N corresponding to the Nth kilobyte of the file. Must be a long to work correctly with some system calls.
- `char haveFile`: a boolean value indicating whether the message is a file request or a file existence query. When the message is a file request and the client expects a kilobyte of the file's data back, set to 0; when the message is a file existence query and the client just wants to know if the file exists on the server, set to 1.

#### Server messages

A `serverMessage` struct has the following fields.

- `long positionInFile`: an integer indicating the position that the data in the file came from. Should be an integer N corresponding to the Nth kilobyte of the file.
- `int bytesToUse`: the number of bytes in the file that are "real" data; that is, bytes that are not header information or padding. Typically, this will be 1024, unless we have reached the end of the file and there are not 1024 more bytes to send.
- `long fileSize`: the total size of the requested file in bytes.
- `char data[1024]`: the actual data being returned by the server. The server never sends more than 1KB in a message at a time.
- `char hasFile`: a boolean value used to answer file existence queries. Indicates whether the server has the file specified in the query. Set to 1 if the server has the file and 0 if it does not.

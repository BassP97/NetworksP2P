# NetworksP2P

A peer-to-peer file-sharing service made for our Computer Networking final project.

Compiles with included Makefile - just use `make`. The server listens to port 8080.

## Peer-to-peer structure

Each instance of our peer-to-peer file-sharing service running on a single host has four threads, two client threads and two server threads.

#### Client threads

There are two client threads: a connector thread and a requester thread. The division of the client into a connector thread and a requester thread allows the client to connect to all available servers and simultaneously make requests to all of them.

The connector thread repeated loops over a list of known hosts that may be running the program, attempting to establish connections with their servers. The thread sleeps for 1 second between loops. When a connection is made, the connector thread saves the socket file descriptor associated with the connection in `client_fd_list`, a shared vector with the requester thread. The connector thread continues to loop even after all active hosts have established connections because 1) more hosts may join the network at any time and 2) a host may drop out of the network and come back at any time, and we want the connector thread to be able to re-establish a connection.

The requester thread waits for the human user to input a file to request from the other servers. It then determines which of the other servers have the file and sends requests for chunks of the file to the other servers until it has received the entire file. The user can then request more files.

#### Server threads

There are two server threads: a listener thread and a reader thread. The division of the server into a listener and a reader thread allows the server to service requests from multiple different clients.

The listener thread sets up a listening port (port 8080) and just waits for clients to make connections. When a new connection comes in, the listener thread accepts it and puts the socket file descriptor for it in an `fd_set` `server_read_fds` and a vector `server_fd_list`. It then returns to listening for a new connection.

The reader thread uses the `server_read_fds` `fd_set` to repeatedly call `select()` on all of the server's open connections. When it receives a request from a client, it determines what part of what file is being requested and sends it back to the requesting client.

## Protocol

Our file-sharing service uses two main message structures and, within each, several types of messages. Clients send messages with the `clientMessage` structure and servers send messages with the `serverMessage` structure. Since clients never communicate with other clients and servers never communicate with other servers, clients can always expect to receive information in a `serverMessage` format and servers can always expect to receive information in a `clientMessage` format. We describe the structure of the client and server messages, as well as the types of information they each carry, below.

#### Client messages

A `clientMessage` struct has the following fields. 

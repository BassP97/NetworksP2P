# NetworksP2P

A peer-to-peer file-sharing service made for our Computer Networking final project.

Compilation: `g++ main.cpp -pthread -std=c++11`. The server listens to port 8080.

### Protocol Message Types

Our protocol has two message structures: messages sent by a client (aka client messages) and messages sent by a server (aka server messages). Both structures can hold multiple types of message.

#### Client message fields
- `char fileName[128]`: 128-byte field to store the file name the client is requesting. Unused if the message is not a file request.
- `long portionToReturn`: 8-byte field to specify the part of the file we want. If this field holds N, the server should give us the Nth kilobyte of the file. Must be a `long` in order to work with some file system calls. Unused if the message is not a file request.

#### Server message fields
- `long positionInFile`: 8-byte field to specify the location in the destination file that the client should write the data to. If this field holds N, the client should write this data to the Nth kilobyte of the file.
- 

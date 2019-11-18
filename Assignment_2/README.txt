TCP Simple Broadcast Chat Server and Client
===============

TCP Echo Server and Client
ECEN 602 Network Programming Assignment 2


Role
===============
Mainly, we worked together, but here is the role what we did.
Minhwan Oh : Developed server programming, debugged for integration 
Sanghyeon Lee : Developed client programming, tested for integration


File info
===============
For this program, you can see how TCP Simple Broadcast Chat Server and Client works. There are two main files. 
1. Client.c
Path: cd Assignment_2/Client.c
Main feature: Client application for chat room
2. Server.c
Path: cd Assignment_2/Server.c
Main feature: Server application for chat room
3. unp.h
Path: cd Assignment _2/unp.h
Main feature: Packages for socket programming function *Refers to Unix Network Programming library*
4. config.h
Path: cd Assignment_2/config.h
Main feature: Autoheader
5. makefile
Main feature: Compile setting description


Build info
===============
Command $make

Program scenario
===============
This program includes client and server for a TCP simple broadcast chat service, which does the following:

1. Start the server first with the commanding lien: $server <IPAdr> <Port> <Accessible Client Number>, where “server” is the name of the server program, IPAdr is the IPv4 (or IPv6, need to switch several lines in code) address of the server in dotted decimal notation, Port is the port number on which the server is listening, and Accessible Client Number is the maximum number of clients can join server at the same time. The server must support multiple simultaneous connections. 

2. Start the client second with a command line: $client <User Name> <IPAdr> <Port>, where client is the name of the client program, User Name is the client’s name which will be used in server as a client ID, IPAdr is the IPv4 (or IPv6, need to switch several lines in code) address of the server in dotted decimal notation, and Port is the port number on which the server is listening. Then, client sends JOIN type to server to join the chat session.

3. When server receives the JOIN type header from a client, the server checks the client’s username is on the existing client list. If the same user name is assigned on the list, the server sends NAK type header to reject its connection. If the user name is new, the server sends ACK type header to the client to confirm the JOIN request and broadcasts the arrival of new participant to the other client who are already in the chat session by sending ONLINE header message.

4. A client sends a message (e.g. Hi guys!) to server, and the server receives the message, then broadcast it to the other clients by using FWD message which contains the message and its username. If header or attribute type is wrong, DISCARD it

5. If a client doesn’t send any message for 10 seconds, the server broadcasts it to the other clients that the user is in IDLE state. However, if the client sends any message again, the IDLE state is over, and the server counts the time again to check the IDLE state of each clients in the chat session.

6. A client leave the session unceremoniously. The server broadcasts OFFLINE message to the other existing clients and cleanup the allocated memory of the left client for new clients can use that user name again.



Program details
===============
1. The communication will work with either IPv4 and IPv6 networks.
1. The server handles all clients’ access by using ACK(accepted) and NAK(rejected) messages and broadcasts the clients’ status (ONLINE(new client joined), OFFLIEN(client left), and IDLE(didn’t move for 10 seconds)) and each other’s messages (SEND and FWD) to other clients.
2. The client ask (JOIN) permission to join chat session and gets the client information of chat session (current client list and the number of client).
3. Clients send and receive messages through server’s FWD system.
4. The server detects clients’ idle status, which clients don’t send any message for 10 seconds, and inform it to the other clients. If the idle clients move, the idle status is over and the server initiates the countdown idle timer to zero.

File architecture 
===============
1. Server
a) Architecture 
	- Scenario : 
		(1) Setup client_info/user_check/broadcast/ACK/NAK/bind/listen functions
		(2) Then going to infinite loop until the exit
	- Main function : 
		(1) int main(int argc, char **argv): Including main scenario
		(2) int check_newFd(int nClient, int nMax_Client, char *username, Client *clientinfo,int connfd): Check new client
(3) void broadcast_all(Message broadcast_Msg,fd_set check_fd, int listenfd, int connfd, int latest_fd): forward clients’ status and messages
b) Feature 
	- Create socket and bind the well-known port of server
	- Check new clients and accept or rejects them
	- Forward clients’ status and messages to the other clients

2. Client
a) Architecture
	- Scenario : 
		(1) Setup JOIN/sendMessage/recvMessage/socket/connect function
(2) Then going to infinite loop until the exit
	- Main function : 
		(1) int main(int argc, char **argv): Including main scenario
		(2) int joinServer(char *buf,int sockfd, int size_buf): Make JOIN in SBCP message format
		(3) int RecvMsg(int sockfd): handle received messages depend on its header type
b) Feature 
	- Create socket and bind the well-known port of server
	- Monitor headers in received message and print it.


Test cases
===============
Current program passed below test cases
(1) Normal operation of the chat client with three clients connected
(2) Server rejects a client with a duplicate username
(3) Server allows a previously used username to be reused
(4) Server rejects the client because it exceeds the maximum number of clients allowed
(5) Bonus features
    1) IPv4 and IPv6 compatibility
    2) Feature 1 - ACK, NAK, ONLINE, OFFLINE
    3) Feature 2 - IDLE

Reference
===============
Below functions are referred by network programming library, W. Richard Stevens, Bill Fenner, and Andrew M. Rudoff, Unix Network
Programming, Volume 1, The Sockets Networking API, 3rd Edition

-server.c

-client.c


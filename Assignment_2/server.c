#include	"unp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>

//Protocol Version
#define VERSION 3

#define JOIN 2
#define SEND 4
#define FWD 3
#define ACK 7
#define NAK 5
#define ONLINE 8
#define OFFLINE 6
#define IDLE 9

#define MAX_USERNAME 16
#define MAX_MESSAGE 512
#define MAX_REASON 32
#define CLIENT_COUNT 2

//SBCP Attribute Type
#define ATTR_USERNAME 2
#define ATTR_MESSAGE 4
#define ATTR_REASON 1
#define ATTR_CLIENT_COUNT 3

//Typedef for convenience
typedef struct sockaddr_in sockaddr;

//SBCP Header
struct Header_SBCP
{
    unsigned int vrsn : 9;
    unsigned int type : 7;
    unsigned int length : 16;
};
typedef struct Header_SBCP Header;

//SBCP Attribute
struct Attribute_SBCP
{
    unsigned int type : 16;
    unsigned int length : 16;
    char payload[MAX_MESSAGE];
};
typedef struct Attribute_SBCP Attribute;


/***************************************
 * 
 *  Structure : SBCP Message
 *  Attribute[0] : It contains message 
 *  Attribute[1] : It contains username
 * 
***************************************/
struct Message_SBCP
{
    Header header;
    Attribute attribute[2];
};
typedef struct Message_SBCP Message;

struct Client_Info
{
	char username[16];
	unsigned int fd_num;
};
typedef struct Client_Info Client; 

//Delete target client, and fill the place from next client info
void delete_clientinfo(int currentfd, Client *clientinfo,int nClient)
{
	int i;
	int move =0;

	for(i = 0; i<nClient; i++)
	{
		if(move == 1 )
		{
			clientinfo[i-1].fd_num = clientinfo[i].fd_num ;
			strcpy(clientinfo[i-1].username,clientinfo[i].username);
		}

		if(clientinfo[i].fd_num == currentfd)
		{
			clientinfo[i].fd_num = 0;
			bzero(clientinfo[i].username,sizeof(clientinfo[i].username));
			move = 1;
		}
	}

}

void find_username(int currentfd, Client *clientinfo, int nClient, char *username)
{
	int i;

	for(i = 0; i < nClient; i++)
	{
		if( clientinfo[i].fd_num == currentfd)
		{
			strcpy(username, clientinfo[i].username);
		}
	}
}

int check_newFd(int nClient, int nMax_Client, char *username, Client *clientinfo,int connfd)
{
	int i;
	int result  = 0;

	if( nClient >= nMax_Client)
	{
		result = 2;
	}
	else
	{
		for(i =0; i < nClient; i++)
		{	
			printf("Clientinfo %d th username = %s and username = %s \n",i+1, clientinfo[i].username, username);
			if(strcmp(clientinfo[i].username, username) ==0)
			{
				//Duplicated username
				result = 1;
			}
		}

		if( result != 1)
		{
			strcpy(clientinfo[i].username,username);
			clientinfo[i].fd_num = connfd;
		}
	}		

	return result;
}

//Broad cast msg to other clients
void broadcast_all(Message broadcast_Msg,fd_set check_fd, int listenfd, int connfd, int latest_fd)
{
	int i;
    for( i = 0; i <= latest_fd; i++) 
    {
		//check the broadcasting fd is valid or not
        if (FD_ISSET(i, &check_fd)) 
        {
            // except the listener and ourselves
            if (i != listenfd && i != connfd)//dont broadcast to the original sender and the server
            {
                if (send(i,(void *) &broadcast_Msg,sizeof(broadcast_Msg),0) < 0)
                {
                    printf("[Error] Server : Fail to broadcast message\n");
                }
            }
        }
    }
}

//send ACK message to the new client
void sendACK(int connfd, int nClient, char *username, Client *clientinfo)
{
    Message msg;
	char a[2];
	char clientlist[MAX_MESSAGE];
	strcat(clientlist, "\b\b");
	for(int i=0 ; i<nClient ; i++)
	{
		strcat(clientlist, clientinfo[i].username);
		strcat(clientlist, " ");
	}

    msg.header.vrsn = VERSION;
    msg.header.type = ACK;

    msg.attribute[0].type = ATTR_CLIENT_COUNT; //the payload in the attribute is message
	sprintf( msg.attribute[0].payload , "%d", nClient ) ;
    msg.attribute[0].length = 4 + strlen(msg.attribute[0].payload ); //default 4 bytes + length of client count value

	msg.attribute[1].type = ATTR_USERNAME;	
	strcpy(msg.attribute[1].payload, clientlist);
	msg.attribute[1].length = 4 + strlen(msg.attribute[1].payload);
    
	if(send(connfd,(void *) &msg,sizeof(msg),0) < 0)
	{
		printf("[Error] Server : Fail to send ACK message\n");
	}
}

//send NAK message to the new client
void sendNAK(int connfd,int code)
{
    
    Message msg;
    
	//Reason Attribute Payload should be equal or less than 32
	char strReason[32];
    
	msg.header.vrsn =3;
    msg.header.type =NAK;
    msg.attribute[0].type = ATTR_REASON;

	switch(code)
	{
		case 1:
			//the flag to mark this NAK is for username existed
			strcpy(strReason,"Same username already existed");//
			break;
		case 2:
			strcpy(strReason,"Server : Full of clients");//
			break;
		default:
		break;
	}

 	strcpy(msg.attribute[0].payload, strReason);
    msg.attribute[0].length = 4 + strlen(strReason);//the length of the payload is depends on the message    we write   

	printf("sendNAK reason : %s \n",strReason);
	msg.header.length = 8 + strlen(strReason);

    if(send(connfd,(void *) &msg,sizeof(msg),0) < 0)
	{
		printf("[Error] Server : Fail to send NAK message\n");
	}
	//This socket should be closed due to inproper condition
    close(connfd);
}

int main(int argc, char **argv)
{

	int					listenfd, connfd, nMaxfd, nMaxi, nMax_Client, nReady, i;
	int 				nClient,nClientinfo,recv_check, result;
	char 				broadID[MAX_USERNAME];
	pid_t				childpid;
	socklen_t			Client_Length;
	sockaddr			cliaddr, servaddr;
	fd_set				check_fd, latest_fd;
	Client 				*clientinfo;
    struct hostent* 	hret;

	Message 			msg;
	Message 			broadcast_Msg;

	//As assignment, set argc as 4
	if (argc != 4)
	{
		printf("[Error] Server : Follow those command - ./Server <Server_IP> <Server_Port> <Number of Clients> \n");
		printf("[Error] Server : Program Exit");
		return 0;
	}

	nMax_Client = atoi(argv[3]);

	if( nMax_Client < 1)
	{
		printf("[Error] Server : nMax_Client should be greater than 0 \n");
	}
	else
	{
		//Allocate client info array
		clientinfo = (struct Clientinfo *)malloc(nMax_Client * sizeof(Client));
	}


	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	//listenfd = socket(AF_INET6, SOCK_STREAM, 0);

	//allow multiple connections
	int opt=1; 
	if( setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt,sizeof(opt))<0)
	{
		printf("[Error] Server: Multiple connections on server socket can't create\n");	
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	//servaddr.sin_family      = AF_INET6;
	servaddr.sin_port        = htons(atoi(argv[2]));
    hret = gethostbyname(argv[1]);
	//inet_pton(AF_INET6, argv[1], &servaddr.sin_addr);
    memcpy(&servaddr.sin_addr.s_addr, hret->h_addr,hret->h_length);
	
	bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

	listen(listenfd, LISTENQ);
	printf("[Info] Server: Listening in progress \n");

	nMaxfd = listenfd;		
	printf("Possible number of connection = %d \n", nMaxfd);

	/*******************************************************
	 * 
	 * FD is file descriptor, and can use it to include select.h
	 * below is FD structure function
	 * 
	 * #define	FD_SET(fd, fdsetp)	__FD_SET (fd, fdsetp)
	 * #define	FD_CLR(fd, fdsetp)	__FD_CLR (fd, fdsetp)
	 * #define	FD_ISSET(fd, fdsetp)	__FD_ISSET (fd, fdsetp)
	 * #define	FD_ZERO(fdsetp)		__FD_ZERO (fdsetp) 
	 * 
	 * 
	 ******************************************************/

	//FD initialization	
	FD_ZERO(&check_fd);
	FD_ZERO(&latest_fd);
	FD_SET(listenfd, &latest_fd);

	// Network variables initialization
	nClient = 0;
	nClientinfo = 0;
	recv_check = 0;

	for ( ; ; ) 
	{

		check_fd = latest_fd;		/* structure assignment */
		nReady = select(nMaxfd+1, &check_fd, NULL, NULL, NULL);

		if( nReady < 0)
		{
			printf("[Error] Server: select function doesn't work properly");
		}

		for(i = 0 ; i <= nMaxfd ; i++)  
		{
			if(FD_ISSET(i, &check_fd)) //Check whether the fd is valid or not
			{
				if(i == listenfd) //Data received on main server, it menas new socket is coming
				{
					Client_Length = sizeof(cliaddr);
					connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &Client_Length); //Accept Client

					if( connfd < 0)
					{
						printf("[Error] Server: Failed to accept Socket\n");
					}
					else
					{

						FD_SET(connfd, &latest_fd); //add to FD set

						recv_check = recv(connfd,(Message *)&msg,sizeof(Message),0);	

						result = check_newFd(nClient, nMax_Client,msg.attribute[0].payload,clientinfo,connfd);

						if(result == 0)
						{
							nClient++; 

							if(connfd > nMaxfd)
							{
								nMaxfd = connfd; //new largest fds 
							}

							//printf("[Info] Server: Maxfd = %d Connfd value is %d \n\n", nMaxfd, connfd);
							sendACK(connfd, nClient, msg.attribute[0].payload,clientinfo);
							printf("[Info] Server: New socket accepted\n");							
							printf("[Info] Server: User %s joined chat room \n",msg.attribute[0].payload);						

							msg.header.vrsn = VERSION;
                            msg.header.type = ONLINE;
                            msg.attribute[0].type = ATTR_USERNAME;
                            strcpy(msg.attribute[0].payload,msg.attribute[0].payload);

                            broadcast_all(msg,latest_fd,listenfd,connfd, nMaxfd);
							
						}
						else
						{							
							sendNAK(connfd,result); 
							FD_CLR(connfd, &latest_fd);//clear accept_socket_fd out of							
						}		
						
					}				
					
				}
				else  //Data received
				{
					recv_check = recv(i,(Message *)&msg,sizeof(Message),0);

					if(recv_check <= 0) //Client has exited
					{			

						msg.header.vrsn = VERSION;
						msg.header.type = OFFLINE;
						msg.attribute[0].type = ATTR_USERNAME;
						find_username(i, clientinfo, nClient, broadID);
						strcpy(msg.attribute[0].payload, broadID);
						msg.attribute[0].length = 4 + strlen(broadID);		
						
    					bzero((char*)&msg.attribute[1].payload,sizeof(msg.attribute[1].payload));			

						broadcast_all(msg,latest_fd,listenfd,i, nMaxfd);

						//Remove Client, Clean Resources and Inform Other Clients
						FD_CLR(i,&latest_fd);
						
						delete_clientinfo(i, clientinfo, nClient);

						//Remove Client Name from list
						close(i);
						nClient--;
						printf("[Info] Server: Client [%s] has exited\n",broadID );
				
					}
					else //Message Received
					{				
						// If Header type or attribute type is wrong, discard it			
						if(msg.header.type == SEND && msg.attribute[0].type == ATTR_MESSAGE) //Check if Forward 
						{								
							printf("[MSG] Server: Broadcast ID : %s \n",broadID );					
							printf("[MSG] Server: Broadcasting data - %s\n", msg.attribute[0].payload);

							msg.header.type = FWD;
							msg.attribute[1].type = ATTR_USERNAME;

							find_username(i, clientinfo, nClient, broadID);
							strcpy(msg.attribute[1].payload, broadID);

							msg.attribute[1].length = 4 + strlen(broadID);

							broadcast_all(msg,latest_fd,listenfd,i, nMaxfd);
						}
						else if(msg.header.type == IDLE)
						{				
		
							msg.header.type = IDLE;
							msg.attribute[0].type = ATTR_USERNAME;

							find_username(i, clientinfo, nClient, broadID);
							strcpy(msg.attribute[0].payload, broadID);

							msg.attribute[0].length = 4 + strlen(broadID);

							broadcast_all(msg,latest_fd,listenfd,i, nMaxfd);
							printf("[MSG] Server: ID [%s] is Idle Status\n", broadID);
						}		

					}
				}
			
			} 
		}
	}
	close(listenfd);
	return 0;
}

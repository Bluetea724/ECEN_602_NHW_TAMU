#include	"unp.h"
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>	
#include <unistd.h>
#include <time.h>



//Protocol Version
#define VERSION 3

#define MAX_MESSAGE		512

#define JOIN 2
#define SEND 4
#define FWD 3
#define ACK 7
#define NAK 5
#define ONLINE 8
#define OFFLINE 6
#define IDLE 9

//SBCP Attribute Type
#define ATTR_USERNAME 2
#define ATTR_MESSAGE 4
#define ATTR_REASON 1
#define ATTR_CLIENT_COUNT 3

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
 *  Attribute[0] : It contains message as SendMsg, and contains username when client joins server
 *  Attribute[1] : It contains username
 * 
***************************************/
struct Message_SBCP
{
    Header header;
	
    Attribute attribute[2];
};
typedef struct Message_SBCP Message;

// Request to join server
int joinServer(char *buf,int sockfd, int size_buf)
{
	int status = 0;
    Message msg;

    msg.header.vrsn = VERSION; //set the protocal version is 3
    msg.header.type = JOIN; // join message is type 2
    msg.attribute[0].type = ATTR_USERNAME; //username used in chatting
    msg.attribute[0].length = size_buf + 4;

    bzero((char*)&msg.attribute[0].payload,sizeof(msg.attribute[0].payload));
    bzero((char*)&msg.attribute[1].payload,sizeof(msg.attribute[1].payload));

	strcpy(msg.attribute[0].payload,buf);

    msg.header.length = ( 8 + size_buf ); 

    if (send(sockfd,&msg,sizeof(msg),0) < 0)
    {
        printf("[Error] Client : Failed to join\n");
    }
    printf("The join message has been sent successfully\n");

	sleep(1);
	status = RecvMsg(sockfd);

	return status;
}


void SendMsg(int sockfd)
{
    Message msg;
    int size_buf;
    char buf[MAX_MESSAGE];

    int i;

    fgets(buf,sizeof(buf)-1,stdin);

    size_buf = strlen(buf)-1;

    if (buf[size_buf]=='\n')
    {    
		buf[size_buf]='\0';
    }

    msg.header.vrsn = VERSION;
    msg.header.type = SEND; 
    msg.attribute[0].type = ATTR_MESSAGE; 
    msg.attribute[0].length = ( 4 + size_buf );

    bzero((char*)&msg.attribute[0].payload,sizeof(msg.attribute[0].payload));
    bzero((char*)&msg.attribute[1].payload,sizeof(msg.attribute[1].payload));
    
	for ( i = 0; i < size_buf; i++ )
	{
    	msg.attribute[0].payload[i] = buf[i];
	}
	
	// We should consider sizeof attribute[0] and attribute[1]
	// So, 8 is default value for type and length field, plus consider attribute[0] payload 
	msg.header.length = ( 8 + size_buf );
    
	if (send(sockfd,&msg,sizeof(msg),0)<0)
    {
		printf("[Error] Client : Failed to write to socket\n");
	}
}

void IdleMsg(int sockfd)
{
    Message msg;
    int size_buf;

    msg.header.vrsn = VERSION;
    msg.header.type = IDLE; 
    msg.attribute[0].type = ATTR_MESSAGE; 
    msg.attribute[0].length = 4;

    bzero((char*)&msg.attribute[0].payload,sizeof(msg.attribute[0].payload));
    bzero((char*)&msg.attribute[1].payload,sizeof(msg.attribute[1].payload));
	
	//IdleMsg doesn't send any payload
	msg.header.length = 8;
    
	if (send(sockfd,&msg,sizeof(msg),0)<0)
    {
		printf("[Error] Client : Failed to write to socket\n");
	}
}

//read message from the server
int RecvMsg(int sockfd)
{
    Message msg;
    int status = 0;
    if(recv(sockfd, (Message *) &msg, sizeof(msg),0) < 0)
	{
		printf("[Error] Client : Failed to receive data from server");
	}

	//If header or attribute type is wrong, discard it
	switch(msg.header.type)
	{
		case 3:
			if(msg.attribute[0].type == ATTR_MESSAGE && msg.attribute[1].type == ATTR_USERNAME)
			{
				printf("[MSG] Client : FWD Message[%s] from Username [%s] \n", msg.attribute[0].payload, msg.attribute[1].payload);
				status=0;
			}
			break;

		case 5:
			if( msg.attribute[0].type == ATTR_REASON)
			{
				printf("[MSG] Client : NAK Message[%s] \n", msg.attribute[0].payload);
				status=1;
			}
			break;
		case 6:
			if( msg.attribute[0].type == ATTR_USERNAME)
			{
				printf("[MSG] Client : OFFLINE Message[%s] is now offline. \n", msg.attribute[0].payload);
				
				status=0;
			}
			break;
		case 7:
			if(msg.attribute[0].type == ATTR_CLIENT_COUNT && msg.attribute[1].type == ATTR_USERNAME)
			{
				printf("[MSG] Client : ACK Message[ %s conntion] - Username(s) [  %s] \n", msg.attribute[0].payload, msg.attribute[1].payload);        
				status=0;
			}			
			break;
		case 8:
			if( msg.attribute[0].type == ATTR_USERNAME)
			{
				printf("[MSG] Client : ONLINE Message - Username [%s] newly joined chat room \n", msg.attribute[0].payload);
			
				status=0;
			}
			break;
		case 9:
			if( msg.attribute[0].type == ATTR_USERNAME)
			{
				printf("[MSG] Client : IDLE Message - Username [%s] is IDLE status \n", msg.attribute[0].payload);
        
				status=0;
			}
			break; 

		default:
			//The client should discard any message that is not understood
			break;
	}
    return status;
}

int main(int argc, char **argv)
{
	int					sockfd;
	int 				connfd;
	struct sockaddr_in	servaddr;
	//for IPv6
	struct hostent* hret;
	fd_set readfd;
    int fdmax;
	char username[MAX_MESSAGE];
	int status = 0;
	int Time_Check = 0;

	if (argc != 4)
	{
		printf("[Error] Client : Follow those command - ./Client <Username> <Server_IP> <Server_Port> \n");
		printf("[Error] Client : Program Exit");
		return 0;
	}

	strcpy(username, argv[1]);
	if(strlen(username) > 16)
	{
		printf("[Error] Client : Username should be less or qual to 16\n");
		return 0;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	hret = gethostbyname(argv[2]);
	memcpy(&servaddr.sin_addr.s_addr, hret->h_addr, hret->h_length);
	///Input Port data 
	servaddr.sin_port = htons(atoi(argv[3]));

	connfd = connect(sockfd, (SA *) &servaddr, sizeof(servaddr));
	if (connfd < 0)
	{
		printf("[Error] Client : Failed to connect\n");
	}

	status = joinServer(username,sockfd,strlen(username));
	if( status == 1)
	{
		close(sockfd);
    	printf("[Error] Client : Client close due to fail joining server\n");
		return 0;
	}
	//Clear the socket set
	FD_ZERO(&readfd);
	FD_SET(0,&readfd);
	FD_SET(sockfd,&readfd);


	for(;;)
	{
		//For IDLE Message 	
		struct timeval tv;
		tv.tv_sec = 0;		
		tv.tv_usec = 100000; // 100ms

		fdmax = sockfd;
		if (select(fdmax+1,&readfd,NULL,NULL,&tv)<0)
		{
			printf("[Error] Client : Failed to select\n");
		}
		else
		{

			if (FD_ISSET(sockfd,&readfd))
			{
				if(RecvMsg(sockfd) ==1)
				{
					close(sockfd);				
					break;
				}
			}
        
			if (FD_ISSET(0,&readfd))
			{
				SendMsg(sockfd);	
				Time_Check = 0;		
			}
			
			//IDLE status is more than 10s
			//It only sends one time after this client is free
			if( Time_Check == 100)
			{
				IdleMsg(sockfd);
			}
			Time_Check++;
		}

	
		FD_SET(0,&readfd);
		FD_SET(sockfd,&readfd);
	}

	close(sockfd);
    printf("[Info] Client : Client close\n");

	return 0;
}

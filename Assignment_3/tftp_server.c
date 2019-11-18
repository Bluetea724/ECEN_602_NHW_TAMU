/*
*TFTP Server
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <signal.h>


#define MAX_DATA_SIZE 512
#define MAX_ERR_SIZE 512
#define MAX_PORT_NUMBER 65535
#define MAX_PORT_NUMBER_SIZE 7
#define MAX_BUFFER_SIZE 2048
#define MAX_FILE_PATH 1024
#define OPCODE_SIZE 2
#define BLK_LIMIT 65536

#define NETASCII "netascii"
#define OCTET "octet"
#define RETRY_LIMIT 10

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;

//Packet structures
struct _ERRPAKT
{
    uint16_t opcode;
    uint16_t errCode;
    char errMsg[MAX_ERR_SIZE];
};
typedef struct _ERRPAKT ERRPAKT;

struct _ACKPAKT
{
    uint16_t opcode;
    uint16_t blkNumber;
};
typedef struct _ACKPAKT ACKPAKT;

struct _REQPAKT
{
    uint16_t opcode;
    char fileName[MAX_FILE_PATH];
};
typedef struct _REQPAKT REQPAKT;

struct _DATAPAKT
{
    uint16_t opcode;
    uint16_t blkNumber;
    char data[MAX_DATA_SIZE];
};
typedef struct _DATAPAKT DATAPAKT;


//Error Message 
enum {
    NOT_DEF,
    FILE_NOT_FOUND,
    ACCESS_VIOLATION,
    DISK_FULL_ALLOCATION_EXCEEDED,
    ILLEGAL_TFTP_OPER,
    UNKNOWN_TRANSFER_ID,
    FILE_ALREADY_EXISTS,
    NO_SUCH_USER
};

static char *errMsg[] = {
    "Not defined",
    "File not found",
    "Access violation",
    "Disk full or allocation exceeded",
    "Illegal TFTP operation",
    "Unknown transfer ID",
    "File already exists",
    "No such user"
};

//Transmission mode
enum {
    MODE_NETASCII,
    MODE_OCTET
};

enum {
    NONE,
    RRQ,    
    WRQ,   
    DATA,   
    ACK,  
    ERR     
};


// print errors using Error numbers
void ErrPrint (const char* Err_Name)
{
    char *errMsg = NULL;
    errMsg = strerror (errno);
    if (NULL != Err_Name)
        printf ("[Error] Server: '%s' error has occured\n", Err_Name);
    printf ("[Error] Server: %s\n", errMsg);
    return;
}


//Signal reap all death process
void sigchld_handler(int s)
{
    int tmp_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = tmp_errno;
}

//Send error message
void ErrSend (int errNum, struct sockaddr_in client_Addr)
{
    struct sockaddr_in child_Addr;
    ERRPAKT errPakt ;                 
    socklen_t addrlen = sizeof (struct sockaddr_in);
    
    int child_fd = 0;
    int errType = FILE_NOT_FOUND;         
    int bufferSize = 0;

    memset (&child_Addr, 0, sizeof (struct sockaddr_in));
    memset (&errPakt, 0, sizeof (ERRPAKT));

   
    if ((child_fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        ErrPrint ("socket");
        return;
    }

    child_Addr.sin_family = AF_INET;
    child_Addr.sin_addr.s_addr = htonl (INADDR_ANY);
    child_Addr.sin_port = htons(0); 

    if (bind(child_fd, (struct sockaddr*) &child_Addr, sizeof (child_Addr)) < 0)
    {
        ErrPrint ("bind");
        return;
    }

    errPakt.opcode = htons(ERR);

    if (errNum == EACCES)
        errType = ACCESS_VIOLATION;

    errPakt.errCode = htons(errType);

    snprintf (errPakt.errMsg, sizeof (errPakt.errMsg), "%s", errMsg[errType]);
    bufferSize = sizeof (errPakt.opcode) + sizeof (errPakt.errCode) + strlen (errPakt.errMsg) + 1;
    sendto (child_fd, (void *)(&errPakt), bufferSize, 0, (struct sockaddr *) &client_Addr, addrlen);
}


//Send data (return 1 for success, 0 for false)
int DataSend (char* fileDir, char* mode_Str, struct sockaddr_in client_Addr)
{
    DATAPAKT dataPakt;                           
    struct sockaddr_in child_Addr;
    struct timeval tv;                               
    ACKPAKT ackPakt;                            
    int retryCnt = 0;        
    int pakcnt = 1;                     
    int mode = MODE_NETASCII;                       
    int child_fd = 0;                           
    int status = 0; 
    int totalData = 0;
    char character = '\0';                          
    char next_Char = -1;                               
    // takes care of '\r' and '\n' in ascii mode

    char buffer [MAX_DATA_SIZE] = {0};
    bool isEOFReached = false;                     
    FILE *fp = NULL;
    socklen_t addrlen = sizeof (struct sockaddr_in);
    uint16_t blockCnt = 1;                           
    uint16_t cnt = 0;                      
    uint16_t buffLen = 0;
    uint16_t buffIndex = 0;
    size_t bufferSize = 0;
    fd_set readFds;
    

    if ((NULL == mode_Str) || (NULL == fileDir))
    {
        printf ("[Error] Server: file Name / sockAddr sent is NULL \n");
        return 0;
    }
    
    memset (&dataPakt, 0, sizeof (DATAPAKT));
    memset (&ackPakt, 0, sizeof (ACKPAKT));
    memset (&child_Addr, 0, sizeof (struct sockaddr_in));
    
    if (0 == strcmp (mode_Str , OCTET))
    {
        mode = MODE_OCTET;
    }

    fp = fopen(fileDir, "r");

    if (NULL == fp)
    {
        ErrSend (errno, client_Addr);
        return 0;
    }
    // Start to send the data-----------------------
    if ((child_fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        ErrPrint ("socket");
        return 0;
    }
    printf("Sending file...\n");

    // Child server
    child_Addr.sin_family = AF_INET;
    child_Addr.sin_addr.s_addr = htonl (INADDR_ANY);
    child_Addr.sin_port = htons(0);

    if (bind(child_fd, (struct sockaddr*) &child_Addr, sizeof (child_Addr)) < 0)
    {
        ErrPrint ("bind");
        return 0;
    }

    FD_ZERO (&readFds);
    FD_SET (child_fd, &readFds);
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    while (1)
    {
        // Copy 512 bytes of data or less
        for (cnt = 0; cnt < MAX_DATA_SIZE; cnt++)
        {
            if ((buffIndex == buffLen) && (false == isEOFReached))
            {
                memset (buffer, 0, sizeof (buffer));   
                buffIndex = 0;
                buffLen = fread (buffer, 1, sizeof (buffer), fp);
                totalData = totalData + buffLen;
                
                if (buffLen < MAX_DATA_SIZE)
                    isEOFReached = true;
                if (ferror (fp))
                    printf ("[Error] Server: Read error\n");
            }
            if (next_Char >= 0) {
                dataPakt.data [cnt] = next_Char;
                next_Char = -1;
                continue;
            }
            // take care of the data set which is less than MAX_DATA
            if (buffIndex >= buffLen)
            {
                break;
            }
            character = buffer [buffIndex];
            buffIndex ++;
            if (MODE_NETASCII == mode)
            {
                if ('\n' == character)
                {
                    next_Char = character;
                    character = '\r';
                }
                else if ('\r' == character)
                {
                    next_Char = '\0';
                }
                else
                {
                    next_Char = -1;
                }
            }
            dataPakt.data [cnt] = character;
        }
	    dataPakt.opcode = htons (DATA);
        dataPakt.blkNumber = htons (blockCnt % BLK_LIMIT);
        bufferSize = sizeof(dataPakt.opcode) + sizeof (dataPakt.blkNumber) + (cnt);
        do 
        {
            sendto (child_fd, (void *)(&dataPakt), bufferSize, 0, (struct sockaddr *) &client_Addr, addrlen);
            if ((status = select (child_fd + 1, &readFds, NULL, NULL, &tv)) <= 0)
            {
                retryCnt ++;
                continue;
            }
            else
            {
	       if ((status = recvfrom (child_fd, (void *)&ackPakt, sizeof (ackPakt), 0, (struct sockaddr*) &client_Addr, &addrlen)) >= 0)
               {
                   if ((ACK == ntohs (ackPakt.opcode)) && (blockCnt == (ntohs (ackPakt.blkNumber))))
                       break; 
               }
            }
            memset (&ackPakt, 0, sizeof (ACKPAKT));
        }while (retryCnt < RETRY_LIMIT);
        if (retryCnt >= RETRY_LIMIT)
        {
            printf ("[Error] Server: Timed out, breaking after %d tries \n", retryCnt);
            break;
        }
        blockCnt++;
        memset (&dataPakt, 0, sizeof (DATAPAKT));
        retryCnt = 0;
        if ((cnt < MAX_DATA_SIZE) && (buffIndex == buffLen))
        {
            break;
        }

        printf("Packet %d acknowledged\n", pakcnt);
        pakcnt++;
    }

    fclose (fp);

    return (1);
}


//Receives file from Client (return 1 for success, 0 for false)
int recvData (char* fileDir, char* mode_Str, struct sockaddr_in client_Addr)
{
    DATAPAKT dataPakt;
    ACKPAKT ackPakt;
    struct sockaddr_in child_Addr;
    struct timeval tv;
    int status = 0;
    int child_fd = 0;
    int mode = MODE_NETASCII;
    char character = '\0';
    FILE *fp = NULL;
    uint16_t blkCnt = 0;
    uint16_t cnt = 0;
    uint16_t byteLen = 0;
    uint16_t buffLen = MAX_DATA_SIZE;
    socklen_t addrlen = sizeof (struct sockaddr_in);
    fd_set readFds;
    bool err_Occr = false;
    bool lastCharWasCR = false;

    if ((NULL == fileDir) || (NULL == mode_Str))
    {
        printf ("[Error] Server: '%s' file Name / sockAddr sent is NULL \n", __FILE__);

        return 0;
    }

    memset (&dataPakt, 0, sizeof (DATAPAKT));
    memset (&ackPakt, 0, sizeof (ACKPAKT));
    memset (&child_Addr, 0, sizeof (struct sockaddr_in));

    if (0 == strcmp (mode_Str , OCTET))
    {
        mode = MODE_OCTET;
    }

    fp = fopen(fileDir, "w");

    if (NULL == fp)
    {
        ErrSend (errno, client_Addr);
        return 0;
    }

    // Start to recv ----------------------
    if ((child_fd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        ErrPrint ("socket");
        return 0;
    }
    
    // Child server
    child_Addr.sin_family = AF_INET;
    child_Addr.sin_addr.s_addr = htonl (INADDR_ANY);
    child_Addr.sin_port = htons(0);

    if (bind(child_fd, (struct sockaddr*) &child_Addr, sizeof (child_Addr)) < 0)
    {
        ErrPrint ("bind");

        return 0;
    }
    printf("Receiving file ...\n");


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
    FD_ZERO (&readFds);
    FD_SET (child_fd, &readFds);
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    while (1)
    {
        ackPakt.opcode = htons (ACK);
        ackPakt.blkNumber = blkCnt;
        sendto (child_fd, (void *)(&ackPakt), sizeof(ackPakt), 0, (struct sockaddr *) &client_Addr, addrlen);
        if ((buffLen != MAX_DATA_SIZE) || (true == err_Occr))
        {
            break;
        }
        if ((status = select (child_fd + 1, &readFds, NULL, NULL, &tv)) <= 0)
        {
            if (0 == status)
            {
                printf ("Server: Timedout while waiting for packet from the host \n");
                break;
            }
            else if (0 > status)
            {
                ErrPrint("select");
                break;
            }
        }
        else
        {
            if ((byteLen = recvfrom (child_fd, (void *)&dataPakt, sizeof (dataPakt), 0, (struct sockaddr*) &client_Addr, &addrlen)) >= 0)
            {
                if (DATA != ntohs (dataPakt.opcode))
                {
                    continue;
                }
                buffLen = byteLen - sizeof(dataPakt.opcode) - sizeof(dataPakt.blkNumber);
                blkCnt = dataPakt.blkNumber;
                
                while (cnt < buffLen)
                {
                    if (MODE_NETASCII == mode)
                    {
                        if (true == lastCharWasCR)
                        {
                            character = dataPakt.data [cnt];
                            if ((0 == cnt) && ('\0' == dataPakt.data [cnt]))
                            {
                                character = '\r';
                            }
                            cnt ++;
                            lastCharWasCR = false;
                        }
                        else if ('\r' == dataPakt.data [cnt])
                        {
                            cnt ++;
                            if (('\n' == dataPakt.data [cnt]) && (cnt < buffLen))
                            {
                                character = dataPakt.data [cnt];
                                cnt ++;
                            }
                            else if (('\0' == dataPakt.data [cnt]) && (cnt < buffLen))
                            {
                                character = dataPakt.data [cnt - 1];
                                cnt ++;
                            }
                            else
                            {
                               if (cnt >= buffLen)
                               {
                                   if (dataPakt.data [cnt - 1] == '\r')
                                   {
                                       lastCharWasCR = true; 
                                   }
                                   continue;
                               } 
                            }
                        }
                        else
                        {
                            character = dataPakt.data [cnt];
                            cnt ++;
                        }
                    }
                    else if (MODE_OCTET == mode)
                    {
                        character = dataPakt.data [cnt];
                        cnt ++;
                    }
                    
                    if (EOF == putc (character, fp))
                    {
                        
                        ErrPrint("putc");
                        printf ("[Error] Server: : There was an error while writing to the file \n");
                        err_Occr = true;
                        break;
                    }
                }
                cnt  = 0;
                memset (&dataPakt, 0, sizeof (dataPakt));
            }
        }
    }

    fclose (fp);

    return 1;
}


//Request handling
void handleRequest (void* buffer, struct sockaddr_in client_Addr, char *tftpFolderPath)
{
    int requestType = NONE;
    REQPAKT *reqPakt = buffer;
    char fileDir [MAX_FILE_PATH]= {0};

    //Parse request
    if ((NULL == buffer) || (NULL == reqPakt))
        return;
    reqPakt->opcode = ntohs(*((uint16_t*)buffer));

  
    if ((NULL == reqPakt) || (NULL == tftpFolderPath))
    {
        ErrPrint ("handleRequest");
        return;
    }

    switch (reqPakt->opcode)
    {

        case WRQ:
            {
                strncat (fileDir, tftpFolderPath, sizeof (fileDir));
                strcat (fileDir, "/");
                strncat (fileDir, reqPakt->fileName, (sizeof (fileDir) - strlen (fileDir)));
                recvData (fileDir, (char *)(buffer + OPCODE_SIZE + strlen (reqPakt->fileName) + 1), client_Addr);
            }
            break;

        case RRQ:
            {
                strncat (fileDir, tftpFolderPath, sizeof (fileDir));
                strcat (fileDir, "/");
                strncat (fileDir, reqPakt->fileName, (sizeof (fileDir) - strlen (fileDir)));
                DataSend (fileDir, (char *)(buffer + OPCODE_SIZE + strlen (reqPakt->fileName) + 1), client_Addr);
            }
            break; 

        default :
            printf ("**Invalid request from the client**\n");
    }
}

int main (int argc, char** argv)
{
    int listenFd = 0;
    int pid = 0;
    int ret = 0;
    struct sigaction sa;
    struct sockaddr_in servAddr;
    struct sockaddr_in client_Addr; 
    socklen_t addrlen = sizeof(struct sockaddr_in);
    char port[MAX_PORT_NUMBER_SIZE] = {0}; 
    char buffer[MAX_BUFFER_SIZE] = {0};   
    char ip[INET_ADDRSTRLEN] = {0};
    
    if (argc != 3)
    {
        printf ("\nArgument Setting Error \n"
                "\nSet Argument : ./tftps Port_Number Repository_Path\n"
                "\nExample : ./tftp_server 5000 Repository/\n");
        return 1;
    }

    if ((listenFd = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        ErrPrint ("socket");
        return 1;
    }

    memset (&servAddr, 0, sizeof (servAddr));
    memset (&client_Addr, 0, sizeof (client_Addr));

    if (atoi (argv[1]) > MAX_PORT_NUMBER)
    {
        printf ("[Error] Server: The Port number exceeded the maximum range 65535 \n"
                "        A port number is within the range of 1025~65535 \n");
        return 1;
    }
    else if (atoi (argv[1]) <= 0)
    {
        printf ("[Error] Server: The Port number is less than 1\n"
                "        A port number is within the range of 1025~65535 \n");
        return 1;
    }

    // Server Information
    servAddr.sin_family = AF_INET;
	//servaddr.sin_family      = AF_INET6;
    servAddr.sin_addr.s_addr = htonl (INADDR_ANY); 
    servAddr.sin_port = htons(atoi(argv[1])); 
	//inet_pton(AF_INET6, argv[1], &servaddr.sin_addr);

    sa.sa_handler = sigchld_handler; 
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    //take care of the scenario where the PORT number provided is already in use
    while (bind(listenFd, (struct sockaddr*) &servAddr, sizeof (servAddr)) < 0)
    {
        // Check for port already in use error
        if (EADDRINUSE == errno)
        {
            printf ("[Port] : %s is already in use by another process, choose any other free port\n", argv [1]);
            printf ("[Port] : ");
            scanf ("%s", port);
            if (MAX_PORT_NUMBER < atoi (port))
            {
                printf ("[Error] Server: The Port number exceeded the maximum range 65535 \n"
                        "        A port number is within the range of 1025~65535 \n");
                return 1;
            }
            else if (0 >= atoi (port))
            {
                printf ("[Error] Server: The Port number is less than 1\n"
                        "        A port number is within the range of 1025~65535 \n");
                return 1;
            }
            servAddr.sin_port = htons(atoi(port));
        }
        else
        {
            ErrPrint ("bind");
            return 1;
        }
    }

    printf("Server: waiting datagram from client . . .\n");
    while (1)
    {
        // Initializing the variable for every loop
        memset (&client_Addr, 0, sizeof (client_Addr));
        memset (&buffer, 0, sizeof (buffer));
        addrlen = sizeof (client_Addr);
        recvfrom (listenFd, (void *)buffer, sizeof(buffer), 0, (struct sockaddr *)&client_Addr, &addrlen);
        inet_ntop( AF_INET, &(client_Addr.sin_addr), ip, INET_ADDRSTRLEN);

        if ((pid = fork()) ==  -1)
        {
            ErrPrint ("fork");
        }
        else if (pid == 0)
        {
            handleRequest ((void *)buffer, client_Addr, argv [2]);
            break;
        }
    }

    return 0; 
}

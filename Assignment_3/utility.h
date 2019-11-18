/*
*TFTP Server Utility Header
*/

#ifndef __TFTP_SERVER_H__
#define __TFTP_SERVER_H__

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
enum {
    MODE_NETASCII,
    MODE_OCTET
};

enum {
    NONE = 0,
    RRQ,     // 1
    WRQ,     // 2
    DATA,    // 3
    ACK,     // 4
    ERR      // 5
};

struct ERRPAKT
{
    uint16_t opcode;
    uint16_t errCode;
    char errMsg[MAX_ERR_SIZE];
};

struct ACKPAKT
{
    uint16_t opcode;
    uint16_t blkNumber;
};

struct REQPAKT
{
    uint16_t opcode;
    char fileName[MAX_FILE_PATH];
};

struct DATAPAKT
{
    uint16_t opcode;
    uint16_t blkNumber;
    char data[MAX_DATA_SIZE];
};



#endif

#include	"unp.h"

#define MAXECHOLINE		4096

/* Fatal error related to system call
 * Print message and terminate */

static int	read_cnt;
static char	*read_ptr;
static char	read_buf[MAXECHOLINE];

static ssize_t my_read(int fd, char *ptr)
{

	if (read_cnt <= 0) {
again:
		if ( (read_cnt = read(fd, read_buf, sizeof(read_buf))) < 0) {
			if (errno == EINTR)
				goto again;
			return(-1);
		} else if (read_cnt == 0)
			return(0);
		read_ptr = read_buf;
	}

	read_cnt--;
	*ptr = *read_ptr++;
	return(1);
}

ssize_t readline(int fd, void *vptr, size_t maxlen)
{
	ssize_t	n, rc;
	char	c, *ptr;

	ptr = vptr;
	for (n = 1; n < maxlen; n++) {
		if ( (rc = my_read(fd, &c)) == 1) {
			*ptr++ = c;
			if (c == '\n')
				break;	/* newline is stored, like fgets() */
		} else if (rc == 0) {
			*ptr = 0;
			return(n - 1);	/* EOF, n - 1 bytes were read */
		} else
			return(-1);		/* error, errno set by read() */
	}

	*ptr = 0;	/* null terminate like fgets() */
	return(n);
}

ssize_t Readline(int fd, void *ptr, size_t maxlen)
{
	ssize_t		n;

	if ( (n = readline(fd, ptr, maxlen)) < 0)
		printf("readline error");
	return(n);
}

/* include writen */
#include	"unp.h"

ssize_t	writen(int fd, const void *vptr, size_t n) /* Write "n" bytes to a descriptor. */
{
	size_t		nleft;
	ssize_t		nwritten;
	const char	*ptr;

	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;		/* and call write() again */
			else
				return(-1);			/* error */
		}

		nleft -= nwritten;
		ptr   += nwritten;
	}
	return(n);
}
/* end writen */

char * Fgets(char *ptr, int n, FILE *stream)
{
	char	*rptr;

	if ( (rptr = fgets(ptr, n, stream)) == NULL && ferror(stream))
		printf("fgets error");

	return (rptr);
}


void Writen(int fd, void *ptr, size_t nbytes)
{
	if (writen(fd, ptr, nbytes) != nbytes)
		printf("writen error");
}

void str_cli(FILE *fp, int sockfd)
{
	char	sendline[MAXECHOLINE], recvline[MAXECHOLINE];
	int nReadline;

	printf("Enter the string\n");
	//Fgets terminated if buf is NULL
	while (Fgets(sendline, MAXECHOLINE, fp) != NULL) 
	{
		printf("Client Sending: %s",sendline);
		//Sending data
		Writen(sockfd, sendline, strlen(sendline));
		
		//Check the length of received data
		nReadline = Readline(sockfd, recvline, MAXECHOLINE);

		//If the length of received data is 0, print out error msg
		if( nReadline == 0)
			printf("str_cli: server terminated prematurely");

		printf("Client Received: %s",recvline);
		printf("Enter the string\n");
	}
	//After terminated, print out this to command window
	printf("Client Sending : Exit \n");
}


int
main(int argc, char **argv)
{
	int					sockfd;
	struct sockaddr_in	servaddr;

	if (argc != 3)
		printf("usage: echo <IPaddress> <Port>");

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	///Input Port data 
	servaddr.sin_port = htons(atoi(argv[2]));
	inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	connect(sockfd, (SA *) &servaddr, sizeof(servaddr));

	str_cli(stdin, sockfd);		/* do it all */

	exit(0);
}

#include	"unp.h"

#define MAXECHOLINE		4096

Sigfunc * signal(int signo, Sigfunc *func)
{
	struct sigaction	act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (signo == SIGALRM) {
#ifdef	SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;	/* SunOS 4.x */
#endif
	} else {
#ifdef	SA_RESTART
		act.sa_flags |= SA_RESTART;		/* SVR4, 44BSD */
#endif
	}
	if (sigaction(signo, &act, &oact) < 0)
		return(SIG_ERR);
	return(oact.sa_handler);
}
/* end signal */

Sigfunc * Signal(int signo, Sigfunc *func)	/* for our signal() function */
{
	Sigfunc	*sigfunc;

	if ( (sigfunc = signal(signo, func)) == SIG_ERR)
		printf("signal error");
	return(sigfunc);
}


ssize_t writen(int fd, const void *vptr, size_t n) /* Write "n" bytes to a descriptor. */						
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

void Writen(int fd, void *ptr, size_t nbytes)
{
	if (writen(fd, ptr, nbytes) != nbytes)
		printf("writen error");
}

void sig_chld(int signo)
{
	pid_t	pid;
	int		stat;

	pid = wait(&stat);
	printf("child %d terminated\n", pid);
	return;
}

void str_echo(int sockfd)
{
	ssize_t		n;
	char		buf[MAXECHOLINE];

again:
	while ( (n = read(sockfd, buf, MAXECHOLINE)) > 0)
	{
		printf("Server: Received : %.*s\n",n, buf);
		//Sending echo data
		Writen(sockfd, buf, n);
		printf("Server: Echoing : %.*s\n",n, buf);
	}

	if (n < 0 && errno == EINTR)
		goto again;
	else if (n < 0)
		printf("str_echo: read error");
}

int main(int argc, char **argv)
{
	int					listenfd, connfd;
	pid_t				childpid;
	socklen_t			clilen;
	struct sockaddr_in	cliaddr, servaddr;
	void				sig_chld(int);
	char 				childaddr[14];

	if (argc != 2)
		printf("usage: echos <Port>");

	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(atoi(argv[1]));

	bind(listenfd, (SA *) &servaddr, sizeof(servaddr));

	listen(listenfd, LISTENQ);
	printf("Server: Listening in progress \n");

	//For cleaning up zombie process, add sig_child func
	Signal(SIGCHLD, sig_chld);

	for ( ; ; ) 
	{
		clilen = sizeof(cliaddr);
		if ( (connfd = accept(listenfd, (SA *) &cliaddr, &clilen)) < 0) 
		{
			if (errno == EINTR)
				continue;		/* back to for() */
			else
				printf("accept error");
		}	
        printf("Server: Connection Accepted \n");

		//Get IP Address
		inet_ntop(cliaddr.sin_family, &cliaddr.sin_addr, childaddr,sizeof(childaddr));
        printf("Server: Connection from %s\n", childaddr);

		if ( (childpid = fork()) == 0) 
		{	
			/* child process */
			//Get Child PID Number
			printf("Server: Child Process created with PID = %d \n", getpid());
			close(listenfd);	/* close listening socket */
			str_echo(connfd);	/* process the request */
			exit(0);
		}
		close(connfd);			/* parent closes connected socket */
	}
}

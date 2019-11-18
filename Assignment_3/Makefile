PROGS = tftp_server
CLEANFILES = *.o
CC = gcc

all:	${PROGS}

tftp_server:	tftp_server.o
		${CC} -o $@ tftp_server.o

clean:
		rm -f ${PROGS} ${CLEANFILES}

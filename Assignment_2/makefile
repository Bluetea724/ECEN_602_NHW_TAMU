PROGS =	server client 
CLEANFILES = *.o
CC = gcc
CFLAGS = -I../lib -g -O2 -D_REENTRANT -Wall

all:	${PROGS}

server:	server.o
		${CC} ${CFLAGS} -o $@ server.o
client:	client.o
		${CC} ${CFLAGS} -o $@ client.o

clean:
		rm -f ${PROGS} ${CLEANFILES}

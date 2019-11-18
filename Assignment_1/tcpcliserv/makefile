PROGS =	Client Server
CLEANFILES = *.o
CC = gcc
CFLAGS = -I../lib -g -O2 -D_REENTRANT -Wall

all:	${PROGS}

echo:	echo.o
		${CC} ${CFLAGS} -o $@ echo.o 
echos:	echos.o
		${CC} ${CFLAGS} -o $@ echos.o

clean:
		rm -f ${PROGS} ${CLEANFILES}

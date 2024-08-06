CC = gcc
CFLAGS = -Wall -O2
LIBS = sph.o -lm

all:
	${CC} ${CFLAGS} -c -o sph.o sph.c
	${CC} ${CFLAGS} -o inv inv.c ${LIBS}
	${CC} ${CFLAGS} -o dir dir.c ${LIBS}
	${CC} ${CFLAGS} -o ang ang.c ${LIBS}
	${CC} ${CFLAGS} -o lin lin.c ${LIBS}

clean:
	rm -f *.o

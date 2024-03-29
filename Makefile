CC := gcc
CFLAGS := -O2 -Wall

.PHONY: all clean

all: btsend btrecv

btsend: main.o btcp.o logging.o help.o
	${CC} -o $@ $^ ${LDFLAGS}

btrecv: main.o btcp.o logging.o help.o
	${CC} -o $@ $^ ${LDFLAGS}

main.o: main.c btcp.h logging.h help.h
	${CC} ${CFLAGS} -c -o $@ $<

btcp.o: btcp.c btcp.h logging.h
	${CC} ${CFLAGS} -c -o $@ $<

logging.o: logging.c logging.h
	${CC} ${CFLAGS} -c -o $@ $<

help.o: help.c help.h
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f btsend btrecv *.o

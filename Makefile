CC := gcc
CFLAGS := -O2 -Wall

.PHONY: all clean

all:
	true

backTCP.o: backTCP.c backTCP.h logging.h
	${CC} ${CFLAGS} -c -o $@ $<

logging.o: logging.c logging.h
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f btsend btrecv *.o

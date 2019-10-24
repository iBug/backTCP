CC := gcc
CFLAGS := -O2 -Wall

.PHONY: all clean

all:
	true

logging.o: logging.c logging.h
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f btsend btrecv *.o

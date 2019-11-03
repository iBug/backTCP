CC := gcc
CFLAGS := -O2 -Wall

.PHONY: all clean

all: btsend btrecv

btsend: main.o btcp.o logging.o
	${CC} -o $@ $^ ${LDFLAGS}

btrecv: btsend
	ln -s $< $@

btcp.o: btcp.c btcp.h logging.h
	${CC} ${CFLAGS} -c -o $@ $<

logging.o: logging.c logging.h
	${CC} ${CFLAGS} -c -o $@ $<

clean:
	rm -f btsend btrecv *.o

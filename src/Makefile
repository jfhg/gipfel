CC=gcc
CFLAGS=-g
LDFLAGS=-g

OBJECTS=panorama.o

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c

panorama: $(OBJECTS)
	$(CC) -o panorama $(OBJECTS) -lm

clean:
	rm -f panorama $(OBJECTS)

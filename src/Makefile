CC=gcc
CPP=c++
CFLAGS=-g -I/usr/X11R6/include
LDFLAGS=-g

OBJECTS=panorama.o

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c

%.o: %.cxx
	$(CPP) -c $(CFLAGS) $*.c

panorama: $(OBJECTS)
	$(CC) -o panorama $(OBJECTS) -lm

flmountains: $(flmountains.o)
	$(CPP) -o flmountains flmountains.o -L/usr/X11R6/lib -lm -lfltk -lfltk_images

clean:
	rm -f panorama $(OBJECTS)

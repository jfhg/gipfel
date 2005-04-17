CC=gcc
CPP=c++
CFLAGS=-g -I/usr/X11R6/include
CXXFLAGS=-g -I/usr/X11R6/include -fno-stack-protector
LDFLAGS=-g -fno-stack-protector

OBJECTS=flmountains.o GipfelWidget.o Mountain.o Panorama.o

#%.o: %.c
#	$(CC) -c $(CFLAGS) $*.c

%.o: %.cxx
	$(CPP) -c $(CXXFLAGS) $*.cxx

gipfel: $(OBJECTS)
	$(CC) -fno-stack-protector -o gipfel $(OBJECTS) -L/usr/X11R6/lib -lm -lfltk -lfltk_images

flmountains: $(flmountains.o)
	$(CPP) -o flmountains flmountains.o -L/usr/X11R6/lib -lm -lfltk -lfltk_images

clean:
	rm -f gipfel $(OBJECTS)

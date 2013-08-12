CC=gcc
CFLAGS=-Wall -pedantic -std=gnu99
DEBUGFLAGS=-Wall -pedantic -std=gnu99 -g

make: all

all: qshell

dist: gzip

gzip: tarball
	gzip qshell.tar
    
tarball: clean
	tar -cvf qshell.tar qshell.c qshell.1 makefile
    
clean: qshell
	rm -f *.o qshell

qshell: qshell.o 
	$(CC) $(CFLAGS) -o qshell qshell.o
	chmod a+x qshell
    
qshell.o: qshell.c
	$(CC) $(CFLAGS) -c qshell.c

debug: gdb.o
	$(CC) $(DEBUGFLAGS) -o qshell qshell.o
	chmod a+x qshell

gdb.o: qshell.c
	$(CC) $(DEBUGFLAGS) -c qshell.c
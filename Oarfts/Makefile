CC = gcc
CFLAG = -ggdb 
LIBD = -L../shared/ -L./
INCD = -I../shared 

all: server test lib
	rm *.o

lib: transfer fileoperation connection byteorder
	ar rcs -o $(OBJD)libfts.a $(OBJD)fileoperation.o $(OBJD)transfer.o $(OBJD)byteorder.o $(OBJD)connection.o 

transfer: 
	#$(CC) $(CFLAG) $(INCD) -o $(OBJD)transfer.o -c transfer.c 
	$(CC) $(CFLAG) $(INCD) -DTRANSFER_DEBUG -o $(OBJD)transfer.o -c transfer.c 
fileoperation:
	$(CC) $(CFLAG) $(INCD) -o $(OBJD)fileoperation.o -c fileoperation.c
byteorder:
	$(CC) $(CFLAG) $(INCD) -o $(OBJD)byteorder.o -c byteorder.c
connection: 
	$(CC) $(CFLAG) $(INCD) -o $(OBJD)connection.o -c connection.c 
misc:
	echo "hello world.\nThis file is a sample for attempte of struct data transportion" > sample.txt

server: lib
	$(CC) $(CFLAG) $(INCD) $(LIBD) -o server server.c -lfts -lattr -lutil

test: lib
	$(CC) $(CFLAG) $(INCD) $(LIBD) -o test/test test.c -lfts -lattr -lutil


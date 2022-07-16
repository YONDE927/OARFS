CC = gcc
CFLAG = -ggdb
INC = -I../shared -I../Oarfts -I.
LIBD = -L../shared -L../Oarfts
MACRO = -DRAW

#remotefs
all: conn mirror record cache fts shared mirrorserver
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o RemoteFS remotefs.c $(OBJD)conn.o $(OBJD)mirror.o $(OBJD)record.o $(OBJD)cache.o -lfuse3 -lpthread -lutil -lfts -lattr -lpq
	rm $(OBJD)*.o

#conn
conn:
	$(CC) $(CFLAG) $(INC) $(MACRO) -o $(OBJD)conn.o -c conn.c 
#mirror
mirror:
	$(CC) $(CFLAG) $(INC) $(MACRO) -o $(OBJD)mirror.o -c mirror.c
#record
record: 
	$(CC) $(CFLAG) $(INC) -o $(OBJD)record.o -c record.c
#cache
cache: 
	$(CC) $(CFLAG) $(INC) -o $(OBJD)cache.o -c cache.c
#fts
fts: 
	make -C ../Oarfts/
#shared
shared: 
	make -C ../shared/


#test
test: test-conn test-mirror 
	echo "Hello connRead test." > /tmp/connRead.txt
	touch /tmp/connWrite.txt
test-config: fts shared 
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o test/test-config test/test-config.c remotefs.c conn.c mirror.c record.c -lfuse3 -lpthread -lutil -lfts -lattr -lpq
test-record: shared 
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o test/test-record test/test-record.c record.c -lattr -lpq -lutil
test-conn: conn fts
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o test/test-conn test/test-conn.c $(OBJD)conn.o -lpthread -lfts -lutil -lattr 
test-mirror: mirror fts
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o test/test-mirror test/test-mirror.c $(OBJD)mirror.o $(OBJD)conn.o -lpthread -lfts -lutil -lattr -lpq

test-mirror2: shared fts
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o test/test-mirror2 test/test-mirror2.c mirror.c conn.c -lpthread -lfts -lutil -lattr -lpq

test-cache: shared
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o test/test-cache test/test-cache.c cache.c -lutil -lattr -lpq

#mirrorserver
mirrorserver: conn fts mirror
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o mirrorserver mirrorserver.c $(OBJD)conn.o $(OBJD)mirror.o -lpthread -lpq -lutil -lfts -lattr


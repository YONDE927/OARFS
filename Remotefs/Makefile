CC = gcc
CFLAG = -ggdb
INC = -I../shared -I../Oarfts -I.
LIBD = -L../shared -L../Oarfts
MACRO = -DRAW

#remotefs
all: conn mirror record test fts mirrorserver
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o RemoteFS remotefs.c $(OBJD)conn.o $(OBJD)mirror.o $(OBJD)record.o -lfuse3 -lpthread -lutil -lfts -lattr -lpq
	rm $(OBJD)*.o

#conn
conn:
	$(CC) $(CFLAG) $(INC) $(MACRO) -o $(OBJD)conn.o -c conn.c 
#cache
cache:
	$(CC) $(CFLAG) $(INC) -o $(OBJD)cache -c cache.c 
#mirror
mirror:
	$(CC) $(CFLAG) $(INC) $(MACRO) -o $(OBJD)mirror.o -c mirror.c
#record
record: 
	$(CC) $(CFLAG) $(INC) -o $(OBJD)record.o -c record.c
#fts
fts: 
	make -C ../Oarfts/


#test
test: test-conn test-mirror 
	echo "Hello connRead test." > /tmp/connRead.txt
	touch /tmp/connWrite.txt
test-conn: conn fts
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o test/test-conn test/test-conn.c $(OBJD)conn.o -lpthread -lfts -lutil -lattr 
test-mirror: mirror fts
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o test/test-mirror test/test-mirror.c $(OBJD)mirror.o $(OBJD)conn.o -lpthread -lfts -lutil -lattr -lpq

#mirrorserver
mirrorserver: conn fts mirror
	$(CC) $(CFLAG) $(INC) $(LIBD) $(MACRO) -o mirrorserver mirrorserver.c $(OBJD)conn.o $(OBJD)mirror.o -lpthread -lpq -lutil -lfts -lattr


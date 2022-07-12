CC = gcc
CFLAG = -ggdb

all: attr list map config
	ar rcs $(OBJD)libutil.a $(OBJD)list.o $(OBJD)map.o $(OBJD)config.o
	ar rcs $(OBJD)libattr.a $(OBJD)attr.o 
	rm $(OBJD)*.o

attr: 
	$(CC) $(CFLAG) $(INC) -o $(OBJD)attr.o -c $(SRCD)attr.c 
list: 
	$(CC) $(CFLAG) $(INC) -o $(OBJD)list.o -c $(SRCD)list.c
map: 
	$(CC) $(CFLAG) $(INC) -o $(OBJD)map.o -c $(SRCD)map.c
config: 
	$(CC) $(CFLAG) $(INC) -o $(OBJD)config.o -c $(SRCD)config.c

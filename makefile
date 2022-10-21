CC=gcc
CFLAGS= -lrt -lpthread

all: mimicPark
	echo "Done"


manager.o: manager.c PARKING.h linked_list.h hash_table_read.h
simulator.o: simulator.c simulator.h PARKING.h
firealarm.o: firealarm.c firealarm.h PARKING.h

mimicPark: firealarm.o manager.o simulator.o

clean: 
	rm -f mimicPark
	rm -f *.o

.PHONY: all clean

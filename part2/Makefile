CC=g++
CFLAGS=-c -Wall -std=gnu++11
DEPS= utils.h requests.h statistics.h

all: gerador sauna clean

sauna: sauna.o
	$(CC) sauna.o -o sauna -lpthread

sauna.o: sauna.cpp
	$(CC) $(CFLAGS) sauna.cpp -o sauna.o -lpthread

gerador: gerador.o
	$(CC) gerador.o -o gerador -lpthread

gerador.o: gerador.cpp
	$(CC) $(CFLAGS) gerador.cpp -o gerador.o -lpthread

clean:
	rm *.o

CXXFLAGS=-g -Wall
CC=g++

build: server subscriber

server: server.o

subscriber: subscriber.o

clean:
	rm -f server subscriber *.o

pack:
	zip -r 325CA_Popa_CatalinaElena_Tema2.zip *.cpp *.h Makefile
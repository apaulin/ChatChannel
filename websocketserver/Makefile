CC=gcc
LIBS=-libwebsockets
OBJS = server.o

all: $(OBJS)
	gcc -Wall -o wss -ljansson -lwebsockets $(OBJS)
	
clean:
	rm *.o
	
%.o: %.c $(HEADERS)
	gcc -c $< -o $@

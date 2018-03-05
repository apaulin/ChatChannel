CC=gcc
LIBS=-libwebsockets
OBJS = ws_server.o \
       dbase_api_obj.o

all: $(OBJS)
	gcc -Wall -o wss -ljansson -lwebsockets $(OBJS)
	
clean:
	rm *.o
	
%.o: %.c $(HEADERS)
	gcc -Wall -c $< -o $@

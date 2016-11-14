CFLAGS=-Wall

all: chatserver chatclient

chatserver: server_chatsystem.c
	gcc $(CFLAGS) server_chatsystem.c -lpthread -o chatserver
			
chatclient: client_chatsystem.c
	gcc $(CFLAGS) client_chatsystem.c -lpthread -o client

clean:
	rm -f chatserver client


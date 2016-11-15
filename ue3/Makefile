CFLAGS=-Wall

all: chatserver chatclient

chatserver: server_chatsystem.c mychat.h
	gcc $(CFLAGS) server_chatsystem.c -lpthread -o chatserver
			
chatclient: client_chatsystem.c mychat.h
	gcc $(CFLAGS) client_chatsystem.c -lpthread rawio.c -ltermcap -o client
#	gcc $(CFLAGS) socketcl.c -o chatcl -lpthread rawio.c -ltermcap

clean:
	rm -f chatserver client


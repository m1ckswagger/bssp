CFLAGS=-Wall

all: socketserver client

socketserver: socketserver.c
	gcc $(CFLAGS) socketserver.c -lpthread -o socksrv
			
client: client.c
	gcc $(CFLAGS) client.c -lpthread -o client

clean:
	rm -f bkup


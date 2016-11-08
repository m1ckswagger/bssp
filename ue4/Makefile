CFLAGS=-Wall

all: mish thread_mish 

mish: mish.c
	gcc $(CFLAGS) mish.c -o mish
	
thread_mish: thread_mish.c
	gcc $(CFLAGS) thread_mish.c -lpthread -o thread_mish

clean:
	rm -f bkup


CFLAGS=-Wall

all: bkup restore

bkup: bkup.c
	gcc $(CFLAGS) bkup.c -o bkup

restore: restore.c
	gcc $(CFLAGS) restore.c -o restore

clean:
	rm -f bkup restore


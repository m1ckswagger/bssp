CFLAGS=-Wall

all: bkup restore

bkup: export BACKUPTARGET = archive 
bkup: bkup.c
	gcc $(CFLAGS) bkup.c -o bkup

restore: restore.c
	gcc $(CFLAGS) restore.c -o restore

clean:
	rm -f bkup


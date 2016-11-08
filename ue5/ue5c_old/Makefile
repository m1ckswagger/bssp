all: menu disp

menu: menu.c share.h
	gcc -Wall menu.c -o menu -lrt -lpthread

disp: display.c share.h
	gcc -Wall display.c -o disp -lrt -lpthread

clean:
	rm -f menu disp

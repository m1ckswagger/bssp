all: menu disp compute

menu: menu.c share.h
	gcc -Wall menu.c -o menu -lrt

disp: display.c share.h
	gcc -Wall display.c -o disp -lrt

compute: compute.c share.h
	gcc -Wall compute.c -o compute -lrt

clean:
	rm -f menu disp compute is151002_pipe_*

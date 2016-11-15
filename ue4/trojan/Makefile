all: trojan shellserver

trojan: trojan.c
		mkdir -p bin
		gcc -Wall -Wextra -O trojan.c -o bin/trojan

shellserver: shellserver.c
		mkdir -p bin
		gcc -Wall -O shellserver.c -o bin/shellserver -g -g3 -lpthread

all: main

httpd: main.c
	gcc -W -Wall -o httpd httpd.c -lpthread
clean:
	rm httpd

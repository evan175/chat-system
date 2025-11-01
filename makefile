all: server

server: server.c
	gcc -o server server.c -lm

clean:
	rm server
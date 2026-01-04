all: server client

server: server.c common.h
	gcc -o server server.c common.c -lm

client: client.c common.h
	gcc -o client client.c common.c -lm
	

clean:
	rm server client
all: server client

server: server.c common.h common.c cJSON.c json_functions.c json_functions.h
	gcc -o server server.c common.c cJSON.c json_functions.c -lm

client: client.c common.h common.c
	gcc -o client client.c common.c -lm
	

clean:
	rm server client
#include <sys/socket.h>
#include <stdio.h>   
#include <stdlib.h>   
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include "common.h"

struct thread_data{
    int server_socket;
    int client_socket;
};

int make_connect(int socket_id, struct sockaddr_in addr){
    int conn = connect(socket_id, (struct sockaddr*)&addr, sizeof(addr));

    if (conn < 0) {
        perror("Connection to server failed\n");
    }

    return conn;
}

//recv msg thread for clients
void* recv_msg_thr(void* input) {
    int sock_id = ((struct thread_data *)input)->client_socket;

    for (;;)
    {
        if(recv_msg(&sock_id) == NULL) {
            printf("Connection closed, exiting recv thread\n");
            exit(1);
        }
    }
}

//user input then send
void* send_msg_thr(void* input) {
    struct thread_data *input_data = (struct thread_data *) input;

    // printf("Thread: %d, data pointer %p\n", gettid(), input_data);

    int client_socket = input_data -> client_socket;
    int server_socket = input_data -> server_socket;

    printf("%s() client_sock=%d server_sock=%d\n", __FUNCTION__, client_socket, server_socket);

    char msg[MAX_MSG_LEN];
    memset(msg, 0, sizeof(msg));
    char buff[2 * sizeof(int) + MAX_SENDING_LEN]; //[2 (STX), len of msg, msg]

    while(1) {
        if (fgets(msg, sizeof(msg), stdin) == NULL) {
            // EOF or read error
            printf("\nEnd of input. Exiting.\n");
            break;
        }

        // Remove the end-of-line character
        char * eol = strrchr(msg, '\n');
        if (eol) (*eol) = '\0'; 

        //this check is pre header sending version
        // if(check_buff(msg) < 0) {
        //     continue;
        // }

        int msg_len = strlen(msg);
        buff[0] = 2;
        buff[1] = msg_len;

        slice_snd(msg_len, buff, msg, client_socket);
        
        if(strcmp(msg, "Exit") == 0) {
            printf("Exiting\n");
            
            if(server_socket) {
                close(server_socket);
                printf("Closed Server Socket\n");
                return NULL;
            }

            close(client_socket);
            printf("Closed client socket\n");

            return NULL;
        }

        // write_msg(client_socket, msg);

        // memset(msg, 0, sizeof(msg));
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        return 1;
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);

    int socket_id = create_socket();

    struct sockaddr_in bind_args;
    memset(&bind_args, 0, sizeof(bind_args));

    bind_args.sin_family = AF_INET;
    bind_args.sin_addr.s_addr = inet_addr(ip);
    bind_args.sin_port = htons(port);


    if (make_connect(socket_id, bind_args)) {
        perror("Client connection:");
        printf("Client connection failure: sid=%d\n", socket_id);
        exit(1);
    }

    printf("Connected sid=%d\n", socket_id); 

    struct thread_data td;
    td.server_socket = 0;
    td.client_socket = socket_id;

    pthread_t read_thread;
    pthread_t write_thread;

    pthread_create(&write_thread, NULL, send_msg_thr, (void *) &td);
    pthread_create(&read_thread, NULL, recv_msg_thr, (void *)&td);

    pthread_join(write_thread, NULL);
    pthread_join(read_thread, NULL);

    printf("Exiting.\n");
}
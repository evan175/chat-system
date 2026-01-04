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
#include <sys/types.h>
#include <math.h>
#include <stdbool.h>
#include <poll.h>
#include "common.h"

#define MAX_CLIENTS 10

struct pollfd pfds_clients[MAX_CLIENTS];
int fd_count = 0;

int bind_socket(int socket_id, struct sockaddr_in bind_args) {
    int rc = bind(socket_id, (struct sockaddr*) &bind_args, sizeof(bind_args));

    if(rc < 0) {
        printf("Error binding socket\n");
        printf("Errno: %d\n", errno);
    }

    return rc;
}

int accept_conns(int socket_id) {
    static struct sockaddr_in client_addr;
    int length = sizeof(client_addr);
    int client_socket = accept(socket_id, (struct sockaddr *)&client_addr, &length);

    if(client_socket < 0) {
        printf("Error accepting client connection\n");
        printf("Errno: %d\n", errno);
    }

    return client_socket;
}

void* server_accept(struct pollfd * pfds_server_ptr) {
    printf("%s() Waiting for incoming connections ssid=%d\n", __FUNCTION__, pfds_server_ptr->fd);

    while(1) {
        int poll_count = poll(pfds_server_ptr, 1, 10000);
        // printf("server poll: count=%d\n", poll_count);
        //lock here

        if (poll_count < 0) {
            perror("poll");
            exit(1);
        }

        if(fd_count > MAX_CLIENTS - 1) {
            printf("Max clients reached, cannot accept more connections\n");
            sleep(3);
            continue;
        }

        if(pfds_server_ptr->revents & POLLIN ){
            int client_sock = accept_conns(pfds_server_ptr->fd);

            if(client_sock > 0) {
                printf("Connected to new client\n");
            } else {
                continue;
            }

            printf("Adding client %d: fd=%d\n", fd_count, client_sock);
            pfds_clients[fd_count].fd = client_sock;
            pfds_clients[fd_count].events = POLLIN;

            //lock here
            fd_count++;
        }
        
    }
}

//reads msgs from clients and sends them to all other clients
void* server_msg_process(void* input) {
    printf("%s() Waiting for incoming data\n", __FUNCTION__);
    while(1) {
        
        int i;
        for (i = 0; i < fd_count; i++) {
            printf("Setting i=%d fd=%d\n", i, pfds_clients[i].fd);
            pfds_clients[i].events = POLLIN;
        }

        int poll_cnt = poll(pfds_clients, fd_count, 10000); 

        if (poll_cnt < 0) {
            perror("poll");
            exit(1);
        }

        // printf("reading poll: fd_count=%d poll_cnt=%d\n", fd_count, poll_cnt);

        for (i = 0; i < fd_count && (poll_cnt > 0); i++) {
            if (pfds_clients[i].revents & POLLIN) {
                int* client_sock = (int*) &pfds_clients[i].fd;
                
                char* msg = recv_msg((int*) client_sock);
                if (msg == NULL)
                {
                    printf("NULL data from client.  i=%d. Removing this client.\n", i);
                    close(pfds_clients[i].fd);
                    pfds_clients[i] = pfds_clients[fd_count - 1];
                    fd_count--;
                    continue;
                }

                printf("Data found from client.  i=%d msg='%s'\n", i, msg);

                int j;
                for(j = 0; j < fd_count; j++) {
                    if(j != i) {
                        printf("forwarding to client j=%d msg=%s\n", j, msg);
                        int other_client_sock = pfds_clients[j].fd;
                        
                        char buff[2 * sizeof(int) + MAX_SENDING_LEN];
                        buff[0] = 2;
                        buff[1] = strlen(msg);
                        slice_snd(strlen(msg), buff, msg, other_client_sock);
                    }
                }
                free(msg);
            }
        }
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

    bind_socket(socket_id, bind_args);

    if((listen(socket_id, 3)) < 0) { //stop program here if error
        printf("error listening\n");
        printf("Errno: %d\n", errno);
        exit(1);
    }

    struct pollfd pfds_server;
    pfds_server.fd = socket_id;
    pfds_server.events = POLLIN;

    pthread_t accept_thread;
    pthread_t server_process_msg_thread;
    pthread_create(&accept_thread, NULL, (void *) server_accept, &pfds_server);
    pthread_create(&server_process_msg_thread, NULL, (void *) server_msg_process, NULL);

    pthread_join(accept_thread, NULL);
    pthread_join(server_process_msg_thread, NULL);
    
    printf("exiting\n");

}

//Exit str doesnt end other connection

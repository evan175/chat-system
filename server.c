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

#define MAX_MSG_LEN 500
#define MAX_SENDING_LEN 30
#define MAX_REPLY_LEN 300
#define MAX_CLIENTS 10

struct pollfd pfds[MAX_CLIENTS + 1];
int fd_count = 1;

struct thread_data{
    int server_socket;
    int client_socket;
};

int create_socket() {
    int socket_id;
    socket_id = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_id < 0) {
        printf("Error creating socket\n");
        printf("Errno: %d\n", errno);
    }

    return socket_id;
}

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

int make_connect(int socket_id, struct sockaddr_in addr){
    int conn = connect(socket_id, (struct sockaddr*)&addr, sizeof(addr));

    if (conn < 0) {
        perror("Connection to server failed\n");
    }

    return conn;
}

int read_msg(int sock, char* buff) {
    int rc = read(sock, buff, MAX_SENDING_LEN - 1);// have start and end byte to see when message ends
    if(rc < 0) {
        printf("Error reading\n");
    } else {
        buff[rc] = '\0';
    }

    return rc;
}

int write_msg(int sock, char* buff) {
    int wc = write(sock, buff, strlen(buff));

    if(wc < 0) {
        printf("Error writing\n");
    }

    return wc;
}


//checks if buff exceeds max len. pre header sending version
int check_buff(char* buff) {
    size_t len = strlen(buff);

    // Check if the read line contains '\n' => means user typed less than max len
    if (len > 0 && buff[len - 1] == '\n') {
        buff[len - 1] = '\0';  
        len -= 1; // Adjust length to exclude the newline

        return 1;
    } else {
        // No '\n' => the user typed at least MAX_MSG_LEN+1 chars before hitting Enter:
        // We must flush the rest of the line from stdin.
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
            
        }
        printf("Error: input exceeds %d characters. Please try again.\n", MAX_MSG_LEN);
        
        return -1;
    }
}

//receive and assemble msg
char* recv_msg(int* sock_id) {
    char buff[MAX_SENDING_LEN];
    memset(buff, 0, sizeof(buff));
    char msg[MAX_MSG_LEN];
    memset(msg, 0, sizeof(msg));
    
    while(1) {
        int msg_len;
        int curr_len = 0;

        int rc = read_msg(*sock_id, buff);

        msg_len = buff[1];
        
        bool first_read = true;
        int msg_idx = 0;
        while(curr_len < msg_len) {
            //append buff to msg
            int i;
            int str_len;
            if(first_read) {
                i = 2; //skip stx and len bytes
                str_len = strlen(buff) - 2;
                first_read = false;
            } else {
                i = 0;
                str_len = strlen(buff);
            }
            memcpy(msg + msg_idx, buff + i, str_len);
            
            msg_idx += str_len;
            curr_len += str_len;

            // printf("Current msg: %s\n", msg);
            // printf("curr len: %d, msg len: %d\n", curr_len, msg_len);

            if(curr_len >= msg_len) {
                break;
            }

            memset(buff, 0, sizeof(buff));
            rc = read_msg(*sock_id, buff);
        };

        // printf("rc: %i\n", rc);
        if(rc == 0) {
            printf("Connection has closed\n");
            return NULL;
        }

        /* Ensure assembled message is null-terminated and print it. */
        if (msg_idx >= 0 && msg_idx < (int)sizeof(msg)) {
            msg[msg_idx] = '\0';
        } else {
            /* Safety: make sure string is terminated */
            msg[sizeof(msg) - 1] = '\0';
        }

        printf("Message Received: %s\n", msg);
        memset(msg, 0, sizeof(msg));
    }

    char *return_msg = malloc(MAX_MSG_LEN);
    strcpy(return_msg, msg);
    return return_msg;
}

//recv msg thread
void* recv_msg_thr(void* input) {
    int* sock_id;
    sock_id = (int*) input;

    // printf("Thread: %d, data pointer %p\n", gettid(), input_data);

    recv_msg(sock_id);
}

//slices msg and copies sliced portion to to_snd
void slice_msg(int i, bool first_sent, char* to_snd, char* msg) {
    char sliced[MAX_SENDING_LEN + 1];
    for(int idx = 0; idx < MAX_SENDING_LEN; idx++){
        sliced[idx] = msg[idx + i];
    }

    //check if first sent to append msg after stx and msg_len in buff
    if(!first_sent){
        memcpy(to_snd + 2, msg + i, MAX_SENDING_LEN); 
        to_snd[2 + MAX_SENDING_LEN] = '\0';

    } else {
        memcpy(to_snd, msg + i, MAX_SENDING_LEN);
        to_snd[MAX_SENDING_LEN] = '\0';
    }
}

//slices and sends msg in chunks
void slice_snd(int msg_len, char* to_snd, char* msg, int client_socket) {//ask about error handling here
    int i = 0;
    bool first_sent = false;

    while(msg_len > MAX_SENDING_LEN){
        slice_msg(i, first_sent, to_snd, msg);
        
        i += MAX_SENDING_LEN;
        msg_len -= MAX_SENDING_LEN;

        //printf("Current buff: %s\n", buff);

        write_msg(client_socket, to_snd);

        if (!first_sent) first_sent = true;

        memset(to_snd, 0, sizeof(to_snd));
    }

    //send leftover msg
    slice_msg(i, first_sent, to_snd, msg);
    
    // printf("final buff: %s\n", buff);
    /* Don't clear the buffer before sending the final chunk. */
    write_msg(client_socket, to_snd);
}

//user input then send
void* send_msg(void* input) {
    struct thread_data *input_data;
    input_data = (struct thread_data *) input;

    // printf("Thread: %d, data pointer %p\n", gettid(), input_data);

    int client_socket = input_data -> client_socket;
    int server_socket = input_data -> server_socket;

    char msg[MAX_MSG_LEN];
    memset(msg, 0, sizeof(msg));
    char buff[2 * sizeof(int) + MAX_SENDING_LEN]; //[2 (STX), len of msg, msg]

    while(1) {
        if (fgets(msg, sizeof(msg), stdin) == NULL) {
            // EOF or read error
            printf("\nEnd of input. Exiting.\n");
            break;
        }

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

void* server_accept(void* input) {
    int* socket_id = (int*) input;

    while(1) {
        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count < 0) {
            perror("poll");
            exit(1);
        }

        if(fd_count > MAX_CLIENTS - 1) {
            printf("Max clients reached, cannot accept more connections\n");
            sleep(3);
            continue;
        }

        if(pfds[0].revents & POLLIN ){
            int client_sock = accept_conns(*socket_id);

            if(client_sock > 0) {
                printf("Connected to new client\n");
            } else {
                continue;
            }

            pfds[fd_count].fd = client_sock;
            pfds[fd_count].events = POLLIN;

            fd_count++;
        }
        
    }
}

//reads msgs from clients and sends them to all other clients
void* server_msg_process(void* input) {
    while(1) {
        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count < 0) {
            perror("poll");
            exit(1);
        }

        int i;
        for (i = 1; i < fd_count; i++) {
            if (pfds[i].revents & POLLIN) {
                void* client_sock = (void*) &pfds[i].fd;
                //TODO: make recv msg return msg if possible
            }
        }
    }
}


int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s [server|client] <ip> <port>\n", argv[0]);
        return 1;
    }

    char* mode = argv[1];
    char* ip = argv[2];
    int port = atoi(argv[3]);

    int socket_id = create_socket();

    struct sockaddr_in bind_args;
    memset(&bind_args, 0, sizeof(bind_args));

    bind_args.sin_family = AF_INET;
    bind_args.sin_addr.s_addr = inet_addr(ip);
    bind_args.sin_port = htons(port);

    struct thread_data *td = malloc(sizeof(struct thread_data)); 

    if(strcmp(mode, "server") == 0) {
        bind_socket(socket_id, bind_args);

        if((listen(socket_id, 3)) < 0) { //stop program here if error
            printf("error listening\n");
            printf("Errno: %d\n", errno);
        }

        int client_socket = accept_conns(socket_id);

        td->client_socket = client_socket;
        td->server_socket = socket_id;

        if(client_socket > 0) {
            printf("Connected\n");
        }

        // pfds[0].fd = socket_id;
        // pfds[0].events = POLLIN;

        // pthread_t accept_thread;
        // pthread_create(&accept_thread, NULL, (void *) server_accept, (void *) socket_id);
    }

    if(strcmp(mode, "client") == 0) {
        if(make_connect(socket_id, bind_args) >= 0){
            printf("Connected\n");
        }

        

        td->client_socket = socket_id;
    }

    pthread_t read_thread;
    pthread_t write_thread;

    pthread_create(&write_thread, NULL, send_msg, (void *) td);
    pthread_create(&read_thread, NULL, recv_msg_thr, (void *) &td->client_socket);

    pthread_join(write_thread, NULL);
    pthread_join(read_thread, NULL);

    printf("exiting\n");

}

//Exit str doesnt end other connection
//

//TODO:
/*make server supports multiple clients, when receives msg from a client send it to all other 
clients. server shouild have list of client sockets. use poll(). 1 thread for waiting for 
new conns, 1 thread for reading and wiritng to other sockets. or 1 thread with poll() 
timeout and call poll() on clients . no keyboard input for server. keep list of socket 
clients

for server, create thread for reading from clients (recv_msg) then writing that client msg to 
all other clients (slice_snd) and thread for listening and storing client sockets in list 
(accept_conns). for client, same as now.

in accepting conns thread for server, infinite loop accpeitng and adding client socks to 
global arr. in reading thread for server, use poll() on client socks arr with timeout, if data
available read from that sock and send to all other socks in arr.
*/
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
#include <stdbool.h>
#include "common.h"

#define MAX_MSG_LEN 500
#define MAX_SENDING_LEN 30

int create_socket() {
    int socket_id;
    socket_id = socket(AF_INET, SOCK_STREAM, 0);

    if(socket_id < 0) {
        printf("Error creating socket\n");
        printf("Errno: %d\n", errno);
    }

    return socket_id;
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
    
    
    int msg_len;
    int curr_len = 0;

    int rc = read_msg(*sock_id, buff);

    if(rc == 0) {
        printf("Connection has closed\n");
        // if the connection to the server is lost, terminate.
        // We could fine a better way.

        return NULL;
    }

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

    /* Ensure assembled message is null-terminated and print it. */
    if (msg_idx >= 0 && msg_idx < (int)sizeof(msg)) {
        msg[msg_idx] = '\0';
    } else {
        /* Safety: make sure string is terminated */
        msg[sizeof(msg) - 1] = '\0';
    }

    printf("Message Received: %s\n", msg);

    char *return_msg = malloc(MAX_MSG_LEN);
    strcpy(return_msg, msg);
    return return_msg;
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
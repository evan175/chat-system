#ifndef COMMON_H
#define COMMON_H

#define MAX_MSG_LEN 500
#define MAX_SENDING_LEN 30
#define MAX_REPLY_LEN 300

int create_socket();
int check_buff(char* buff);
char* recv_msg(int* sock_id);
void slice_snd(int msg_len, char* to_snd, char* msg, int client_socket);

#endif

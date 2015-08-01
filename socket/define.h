#ifndef MAIN_H_
#define MAIN_H_

typedef struct msgbuf {
    long mtype;
    char data[1024];
} msgbuf;

#define IN_PATH  ".in"
#define OUT_PATH ".out"
#define MSGTYPE 100001
#define PREFIX  "I am king. "

#define SERVER_PORT 8999
#define SERVER_IP   "127.0.0.1"
#define REMOTE_IP   "127.0.0.1"
#define REMOTE_PORT 9000
#define MSG_MAX_LEN 1024

#endif

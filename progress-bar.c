#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <time.h>

const int LEN = 20;
const long MSGTYPE = 1000001;
typedef struct msgbuf {
    long mtype;       /* message type, must be > 0 */
    int data;    /* message data */
    double speed;
}msgbuf;

void ReciveKill()
{

    fprintf(stderr, "Recived kill cmd--------------------\n");
    exit(0);
}

int getRandom()
{
    srand(time(NULL));
    
    return (rand() + 1000 )% 1000;
}

int main(int argc, char *argv[])
{
    int msgid;
    key_t key;
    int ret;

    key = ftok("/home/dev1/processing-bar.c", 1024);
    msgid = msgget(key, IPC_CREAT|0666);
    if (msgid < 0) {
        fprintf(stderr, "msgget error[%d]\n", msgid);
        exit(1);
    }
    signal(SIGTERM, ReciveKill);
    signal(SIGQUIT, ReciveKill);
    signal(SIGINT,  ReciveKill);

    ret = fork();
    if (ret == 0) {
        int i;
        msgbuf buf;
        buf.mtype = MSGTYPE;
        for (i = 1; i <= LEN; i++) {
            buf.data = i;
            buf.speed = getRandom() / 100.00;
            ret = msgsnd(msgid, &buf, sizeof(buf), IPC_NOWAIT);
            if (ret < 0) {
                fprintf(stderr, "snd msg error[%d]\n", ret);
            }
            sleep(1);
        }
        exit(0);
	}

    sleep(1);
    ret = fork();
    if (ret == 0) {
        int i;
        int process;
        msgbuf buf;
        while (1) {
            ret = msgrcv(msgid, &buf, sizeof(buf), MSGTYPE, 0);
            if (ret < 0) {
                fprintf(stderr, "recv msg error[%d]\n", ret);
                exit(1);
            }
            process = buf.data;

            printf("%3d%% [", process);
            for (i = 0; i < LEN; i++) {
                if (i < process) {
                    printf("=");
                } else {
                    printf(" ");
                }
            }
            printf("]  %.2fk/s", buf.speed);
            fflush(stdout);
            if (process < LEN) {
                printf("\r");
            } else {
                printf("\n");
                break;
            }
        }
        exit(0);
    }

    wait(NULL);
    return 0;
}

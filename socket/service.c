#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <errno.h>
#include "define.h"

int main(int argc, char *argv[])
{
	int in_msgid, out_msgid;
	int ret;
	msgbuf buf;

	memset(&buf, 0x00, sizeof(buf));

	key_t in_key = ftok(IN_PATH, 0);
	fprintf(stdout, "key: %d\n", in_key);

	if ((in_msgid = msgget(in_key, IPC_CREAT | 0666)) < 0) {
		fprintf(stderr, "create or open queue error\n");
		exit(-1);
	}
	key_t out_key = ftok(OUT_PATH, 0);
	if ((out_msgid = msgget(out_key, IPC_CREAT | 0666)) < 0) {
		fprintf(stderr, "create or open queue error\n");
		exit(-1);
	}
	while (1) {
		ret = msgrcv(in_msgid, &buf, sizeof(buf), MSGTYPE, 0);
		if (ret < 0) {
			fprintf(stderr, "[ERROR]: rcv msg error, [%d][%s]\n", errno, strerror(errno));
			continue;
		}
		fprintf(stdout, "[MESSAGE]: [%s]\n", buf.data);
		strcat(buf.data, PREFIX);

		ret = msgsnd(out_msgid, &buf, sizeof(buf.data), IPC_NOWAIT);
		if (ret< 0) {
			fprintf(stderr, "[ERROR]: send msg error, [%d][%s]\n", errno, strerror(errno));
			continue;
		}
	}
	
	return 0;
    
}

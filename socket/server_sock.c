#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/select.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "define.h"

int client_status = 0;
int server_status = 0;

int set_file_lock(int fd)
{
	struct flock fl;
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	return fcntl(fd, F_SETLK, &fl);
}

int set_file_unlock(int fd)
{
	struct flock fl;
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0;
	return fcntl(fd, F_SETLK, &fl);
}

int add_server(fd_set *rset, int *maxfd, int *ssocket)
{
	int on;
	int flag;
	int server_sock;
	struct linger slinger;
	struct sockaddr_in server_addr;

	if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		return errno;

	flag = fcntl(server_sock, F_GETFL, 0);
	fcntl(server_sock, F_SETFL, flag | O_NONBLOCK);

	slinger.l_onoff = 1;
	slinger.l_linger = 0;
	setsockopt(server_sock, SOL_SOCKET, SO_LINGER, &slinger, sizeof(slinger));

	on = 1;
	setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
	if (bind(server_sock, (const struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
		close(server_sock);   
		return -1;
	}

	if (listen(server_sock, 5) == -1) {
		close(server_sock);   
		return -1;
	}

	FD_SET(server_sock, rset);
	if (server_sock > *maxfd)
		*maxfd = server_sock;

	*ssocket = server_sock;

	return 0;
}

int active_client_socket(int *client_socket)
{
	int sock;
	int flag;
	struct sockaddr_in server;
	struct linger clinger;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	if ((flag = fcntl(sock, F_GETFL, 0)) < 0) {
		close(sock);
		return -1;  
	}

	fcntl(sock, F_SETFL, flag | O_NONBLOCK);
	clinger.l_onoff = 1;
	clinger.l_linger = 0;

	setsockopt(sock, SOL_SOCKET, SO_LINGER, &clinger, sizeof(clinger));
	server.sin_addr.s_addr = inet_addr(REMOTE_IP);
	server.sin_family = AF_INET;
	server.sin_port = htons(REMOTE_PORT);

	*client_socket = sock;
	return connect(*client_socket, (struct sockaddr *)&server, sizeof(server));
}

int add_client(fd_set *wset, int *maxfd, int *csocket)
{
	int client_socket;
	int ret;

	ret = active_client_socket(&client_socket);
	if (ret) {
		return -1;
	}
	client_status = 1;
		FD_SET(client_socket, wset);
		if (client_socket > *maxfd)
			*maxfd = client_socket;

		*csocket = client_socket;
		return 0;
}

int create_msgq(int *in_msgid, int *out_msgid)
{
	key_t out_key;
	key_t in_key;

	if ((out_key = ftok(IN_PATH, 0)) < 0 || (in_key = ftok(OUT_PATH, 0)) < 0) {
		fprintf(stderr, "[ERROR]: get msgqid error");
		return -1;
	}

	*in_msgid = msgget(out_key, IPC_CREAT | 0666);
	*out_msgid = msgget(in_key, IPC_CREAT | 0666);
	if (*in_msgid < 0 || *out_msgid < 0)
		return -1;

	return 0;
}

int send_idle_message(int csocket)
{
	if (client_status) {
		write(csocket, "0000", 4);
	}
	return 0;
}

int check_ip_and_accept(int server_sock, fd_set *fset, int *maxfd, int *new_socket)
{
	unsigned int len;
	char remote_addr[16];
	struct sockaddr_in addr;
	
	*new_socket = accept(server_sock, (struct sockaddr *)&addr, &len);
	if (*new_socket < 0) {
		return -1;
	}

	strcpy(remote_addr, inet_ntoa(addr.sin_addr));
	if (strcmp(remote_addr, REMOTE_IP)) {
		close(*new_socket);
		return -1;
	}
	FD_SET(*new_socket, fset);
	if (*new_socket > *maxfd)
		*maxfd = *new_socket;
	return 0;
}

int main(int argc, char *argv[])
{
	int lock_fd, maxfd = -1;
	int ret, flag;
	int pid;
	int in_msgqid, out_msgqid;
	int sock_to_service[2], service_to_sock[2];
	fd_set rset, wset, rs, ws;
	char buf[1024];
	msgbuf msg_buf;
	int select_times, send_idle_times;
	int client_socket, server_socket, new_server_socket;

	if (argc != 2) {
		fprintf(stderr, "[ERROR]: parameter error\n");
		exit(-1);
	}

	lock_fd = open(argv[1], O_RDWR | O_CREAT);
	ret = set_file_lock(lock_fd);
	if (ret) {
		fprintf(stdout, "[ERROR]: file hash been locked\n");
		exit(-1);
	}

	ret = create_msgq(&in_msgqid, &out_msgqid);
	if (ret) {
		fprintf(stderr, "[ERROR]: get msgqid error\n");
		exit(-1);
	}

	if (pipe(sock_to_service) == -1) {
		fprintf(stderr, "[ERROR]: create pipe sock_to_service error\n");
		exit(-1);
	}

	pid = fork();   // The first child process, read pipe and send to msgq
	if (pid == -1) {
		fprintf(stderr, "[ERROR]: create child process error\n");	
		exit(-1);
	} else if (pid == 0) {
		while (1) {
			memset(buf, 0x00, sizeof(buf));
			ret = read(sock_to_service[0], buf, sizeof(buf));   
			if (ret == -1)
				exit(-1);
			else if (ret == 0)
				continue;

			fprintf(stdout, "[MSG]: read msg from sock pipe [%s]\n", buf);
			msg_buf.mtype = MSGTYPE;
			strncpy(msg_buf.data, buf, MSG_MAX_LEN);
			ret = msgsnd(out_msgqid, &msg_buf, sizeof(msg_buf.data), 0);
			if (ret < 0) {
				fprintf(stderr, "[ERROR]: snd msg error. [%d][%s]\n", errno, strerror(errno));
			}
		}
	}

	if (pipe(service_to_sock) == -1) {
		fprintf(stderr, "[ERROR]: create pipe service_to_sock error\n");
		exit(-1);
	}

	flag = fcntl(service_to_sock[0], F_GETFL, 0);
	fcntl(service_to_sock[0], F_SETFL, flag | O_NONBLOCK);

	FD_SET(service_to_sock[0], &rset);
	if (service_to_sock[0] > maxfd)
		maxfd = service_to_sock[0];

	pid = fork();   // The second child process, read from msgq and send to pipe
	if (pid == -1) {
		fprintf(stderr, "[ERROR]: create child process error\n");	
		exit(-1);
	} else if (pid == 0) {
		while (1) {
			memset(&msg_buf, 0x00, sizeof(msg_buf));
			ret = msgrcv(in_msgqid, &msg_buf, sizeof(msg_buf), MSGTYPE, 0);
			if (ret < 0) {
				fprintf(stderr, "[ERROR]: recv msg error. [%d][%s]\n", errno, strerror(errno));
				continue;
			}
			fprintf(stdout, "[MSG]: server recved data from msgq [%s]\n", msg_buf.data);
			strncpy(buf, msg_buf.data, MSG_MAX_LEN);
			ret = write(service_to_sock[1], buf, sizeof(buf));
			if (ret != sizeof(buf)) {
				fprintf(stderr, "[ERROR]: snd buf to pipe error. [%d][%s]\n", errno, strerror(errno));
			}
		}
	}

	/*
	 * signal
	 */
	ret = add_server(&rset, &maxfd, &server_socket);
	if (ret) {
		fprintf(stderr, "[ERROR]: Add server error\n");
		return -1;
	}
	ret = add_client(&wset, &maxfd, &client_socket);
	if (ret) {
		fprintf(stderr, "[ERROR]: Create client error\n");
	}

	select_times = 0;
	send_idle_times = 0;

	struct timeval time_out;

	while (1) {
		time_out.tv_sec = 10;
		time_out.tv_usec = 0;
		rs = rset;
		ws = wset;
		ret = select(maxfd + 1, &rs, &ws, NULL, &time_out);
		if (ret < 0 && errno != EINTR)
			continue;

		if (0 == client_status) {
			ret = add_client(&wset, &maxfd, &client_socket);
			if (ret) {
				fprintf(stderr, "[ERROR]: Create client error\n");
			} else
				client_status = 1;
		}

		if (0 == ret)
			send_idle_times++;
		else
			send_idle_times = 0;

		select_times++;
		if (FD_ISSET(service_to_sock[0], &rs)) {
			if (read(service_to_sock[0], buf, sizeof(buf)) < 0) {
				fprintf(stderr, "[ERROR]: read from service_to_sock error\n");  
			} else {
				if (0 == client_status) {
					ret = add_client(&wset, &maxfd, &client_socket);
					if (ret) {
						fprintf(stderr, "[ERROR]: Create client error\n");
					} else 
						client_status = 1;
				}
				if (1 == client_status) {
					ret = write(client_socket, buf, strlen(buf));
					if (-1 == ret)
						fprintf(stderr, "[ERROR]: send message error\n");
				}
			}
		}

		if (FD_ISSET(server_socket, &rs)) {
			if (0 == server_status) {
				if (check_ip_and_accept(server_socket, &rs, &maxfd, &new_server_socket) < 0)
					fprintf(stderr, "[ERROR]: invalued ip access, deny\n");
			} else 
				server_status = 1;
		}

		if (FD_ISSET(new_server_socket, &rs)) {
			ret = read(new_server_socket, buf, sizeof(buf));
			if (ret > 0) {
				write(sock_to_service[1], buf, ret);
			}
		}

		if (send_idle_times > 3) {
			send_idle_times = 0;
			send_idle_message(client_socket);
		}
	}

	set_file_unlock(lock_fd);
	
	return 0;
}

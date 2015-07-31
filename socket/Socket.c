#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define SERVER_PORT 8999
#define SERVER_IP   "127.0.0.1"
#define REMOTE_IP   "127.0.0.1"
#define REMOTE_PORT 9000

int client_sock;
int server_socket;

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

int add_server(fd_set *rset, int maxfd)
{
    int on;
    struct linger slinger;
    struct sockaddr_in server_addr;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return errno;

    flag = fcntl(server_socket, F_GETFL, 0);
    fcntl(server_socket, F_SETFL, flag | O_NONBLOCK);

    slinger.l_onoff = 1;
    slinger.l_linger = 0;
    setsockopt(server_socket, SOL_SOCKET, SO_LINGER, &slinger, sizeof(slinger));

    on = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (bind(server_socket, (const struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        close(server_socket);   
        return -1;
    }

    if (listen(server_socket, 5) == -1) {
        close(server_socket);   
        return -1;
    }

    FD_SET(server_socket, rset);
    if (server_socket > *maxfd)
        maxfd = server_socket;

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

    setsockopt(sock, SOL_SOCKET, SO_LIEGER, &clinger, sizeof(clinger));
    server.sin_addr.s_addr = inet_addr(REMOTE_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(REMOTE_PORT);

    *client_socket = sock;
    client_socket = sock;
    return connect(*client_socket, (struct sockaddr *)&server, sizeof(server));
}

int add_client(fd_set &wset, int *maxfd)
{
    int client_socket;
    int ret;

    ret = active_client_socket(&client_socket);
    if (ret) {
        fprintf(stderr, "[ERROR]: active client socket error\n");   
        return -1;
    }
    FD_SET(client_socket, rset);
    if (client_socket > *maxfd)
        *maxfd = client_socket;
    return 0;
}

int main(int argc, char *argv[])
{
    int lock_fd;
    int maxfd;
    int ret;
    int pid;
    int sock_to_service[2], service_to_sock[2];
    fd_set rset, wset, rs, ws;
    char buf[1024];
    int select_times;
    int send_idle_times;
    int maxfd = -1;

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

    if (pipe(sock_to_service) == -1) {
        fprintf(stderr, "[ERROR]: create pipe sock_to_service error\n");
        exit(-1);
    }

    pid = fork();   // The first child process
    if (pid == -1) {
        fprint(stderr, "[ERROR]: create child process error\n");    
        exit(-1);
    } else if (pid == 0) {
        while (1) {
            ret = read(sock_to_service[0], buf, sizeof(buf));   
            if (ret == -1)
                exit(-1);
            else if (ret == 0)
                continue;

            /*
 *           * ret = MsgQSend();
 *                       */
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

    pid = fork();   // The second child process
    if (pid == -1) {
        fprint(stderr, "[ERROR]: create child process error\n");    
        exit(-1);
    } else if (pid == 0) {
        while (1) {
            /*
 *           * ret = MsgQRecv();
 *                       */
            ret = write(service_to_sock, buf, sizeof(buf));
            if (ret != sizeof(buf)) {
        
            }
        }
    }

    /*
 *   * signal
 *       */
    ret = add_server(rset, &maxfd);
    if (ret) {
        fprintf(stderr, "[ERROR]: Add server error\n");
        return -1;
    }
    ret = add_client(wset, &maxfd);
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
        ret = select(maxfd + 1, rs, ws, NULL, &time_out);
        if (ret < 0 && errno != EINTR)
            continue;

        if (0 == ret)
            send_idle_times++;
        else
            send_idle_times = 0;

        select_times++;
        if (FD_ISSET(service_to_sock, &rs)) {
            if (read(service_to_sock[0], buf, sizeof(buf)) < 0) {
                fprintf(stderr, "[ERROR]: read from service_to_sock error\n");  
            } else {
                ret = write(client_sock, buf, strlen(buf));
                if (-1 == ret)
                    fprintf(stderr, "[ERROR]: send message error\n");
            }
        }

        if (FD_ISSET(server))
    }

    if (send_idle_times > 3) {
        send_idle_times = 0;
        send_idle_message();
    }

    set_file_unlock(lock_fd);
    
    return 0;
}

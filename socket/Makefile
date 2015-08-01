all: server_sock service

server_sock: server_sock.c define.h
	gcc -Wall -g server_sock.c -o server_sock

service: service.c define.h
	gcc -Wall -g service.c -o service

clean:
	rm -f server_sock service

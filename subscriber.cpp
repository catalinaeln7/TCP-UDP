#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

using namespace std;

short curr_msg_size = -1;
short copy_msg_size = -1;
char pending_msg[BUFLEN] = "\0";

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

void parse_server_msg(char buffer[]) {
	message_udp *msg_udp = (message_udp *)buffer;
	
	printf("%s:%d - %s - ", inet_ntoa(msg_udp->cli_addr.sin_addr),
							ntohs(msg_udp->cli_addr.sin_port), msg_udp->topic);

	switch (msg_udp->data_type) {
		case 0: {
			printf("INT - ");
			bool sign = *msg_udp->content;
			uint32_t num = ntohl(*(uint32_t *)(msg_udp->content + 1));
			if (sign) {
				printf("-");
			}

			printf("%d", num);
			break;
		}
		
		case 1: {
			printf("SHORT_REAL - ");
			uint16_t num = ntohs(*(uint16_t *)(msg_udp->content));
			if (num % 100 == 0) {
				printf("%d", num / 100);
			} else {
				printf("%.2lf", ((double) num) / 100);
			}
			break;
		}

		case 2: {
			printf("FLOAT - ");
			bool sign = *msg_udp->content;
			uint32_t num = ntohl(*(uint32_t *)(msg_udp->content + 1));
			uint8_t pow = *(uint8_t *)(msg_udp->content + 5);
			uint8_t zero = 0;
			uint32_t dec = 0;
			uint32_t pow10 = 1;

			if (sign) {
				printf("-");
			}

			while (pow) {
				dec = dec + (num % 10) * pow10;
				if(num % 10 == 0) {
					zero++;
				} else {
					zero = 0;
				}

				pow--;
				pow10 *= 10;
				num /= 10;
			}
			printf("%d", num);
			if (dec) {
				printf(".");
				while(zero) {
					printf("0");
					zero--;
				}
				printf("%d", dec);
			}
			break;
		}
		case 3: {
			printf("STRING - %s", msg_udp->content);
			break;
		}
	}

	printf("\n");
}

void complete_msg(char buffer[], int nbytes) {

	while(nbytes) {
		if (curr_msg_size == -1) {
			curr_msg_size = *(short *) buffer;
			copy_msg_size = curr_msg_size;
			buffer += 2;
			nbytes -= 2;
		} else if (nbytes < curr_msg_size) {
			memcpy(pending_msg + (copy_msg_size - curr_msg_size),
				   buffer, nbytes);
			curr_msg_size -= nbytes;
			nbytes = 0;
		} else {
			memcpy(pending_msg + (copy_msg_size - curr_msg_size),
				   buffer, curr_msg_size);
			buffer += curr_msg_size;
			nbytes -= curr_msg_size;
			buffer[copy_msg_size] = '\0';
			parse_server_msg(pending_msg);
			pending_msg[0] = '\0';
			curr_msg_size = -1;
		}
	}
}

int parse_tcp_msg(char buffer[]) {
	char *ptr, topic[50];
	int sf;

	ptr = strtok(buffer, " \n");
	if (strcmp(ptr, "subscribe") == 0) {
		ptr = strtok(NULL, " \n");
		if (!ptr) return 0;
		memcpy(topic, ptr, sizeof(ptr));

		ptr = strtok(NULL, " \n");
		if (!ptr) return 0;
		sf = atoi(ptr);

		if (sf < 0 || sf > 1) return 0;

		printf("Subscribed to topic.\n");
		return 1;
	} else if (strcmp(ptr, "unsubscribe") == 0) {
		ptr = strtok(NULL, " \n");
		if (!ptr) return 0;
		memcpy(topic, ptr, sizeof(ptr));

		printf("Unsubscribed from topic.\n");
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockfd, n, ret, exit_while, flag = 1;
	sockaddr_in serv_addr;
	char client_msg[BUFLEN], server_msg[BUFLEN], client_id[11];

	fd_set read_fds;
	fd_set tmp_fds;
	int fdmax;

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	if (argc < 3) {
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	n = setsockopt(sockfd, 
					IPPROTO_TCP,
					TCP_NODELAY,
					&flag,
					sizeof(flag));
	DIE(n < 0, "setsock");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	// trimit id ul clientului
	client_id[0] = strlen(argv[1]);
	memcpy(client_id + 1, argv[1], strlen(argv[1]));

	n = send(sockfd, client_id, strlen(client_id), 0);
	DIE(n < 0, "send");

	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);
	fdmax = sockfd + 2;


	while (!exit_while) {	
		tmp_fds = read_fds;

		ret = select(fdmax, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (int i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					memset(server_msg, 0, BUFLEN);
					n = recv(sockfd, server_msg, sizeof(server_msg) - 1, 0);
					DIE(n < 0, "recv");
					
					if (n == 0) {
						exit_while = 1;
						break;
					}

					complete_msg(server_msg, n);
				} else if (i == 0) {
					memset(client_msg, 0, sizeof(client_msg));
					// se citeste de la tastatura
					n = read(STDIN_FILENO, client_msg, BUFLEN - 1);
					DIE(n < 0, "read");
					
					if (strncmp(client_msg, "exit", 4) == 0) {
						exit_while = 1;
						break;
					}

					char copy[BUFLEN];
					strcpy(copy, client_msg);
					int is_ok = parse_tcp_msg(copy);
					if (!is_ok) continue;

					// se trimite mesaj la server
					short msg_size = strlen(client_msg) - 1;
					client_msg[msg_size] = '\0';
					memmove(client_msg + 2, client_msg, msg_size + 1);
					memcpy(client_msg, &msg_size, sizeof(short));

					n = send(sockfd, client_msg, msg_size + 2, 0);
					DIE(n < 0, "send");
				}
			}
		}
	}

	close(sockfd);

	return 0;
}
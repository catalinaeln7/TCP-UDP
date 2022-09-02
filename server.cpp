#include <bits/stdc++.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "helpers.h"

using namespace std;

struct subscriber {
	char client_id[11] = "";
	int sf;
	vector<string> contents; 
};

struct subscriber_msg {
	char client_id[11] = "";
	short curr_msg_size = -1;
	short copy_msg_size;
	char buffer[BUFLEN] = "\0";
};

struct sock_data {
	int fd;
	sockaddr_in cli_addr;
};

map<string, vector<subscriber>> subscribed_to_topics;

unordered_map<int, subscriber_msg> umap_subscribers;
map<string, int> connected_ids_fds;
vector<sock_data> sock_data_list;

void manage_udp_message(char buffer[], sockaddr_in cliaddr, int nbytes) {
	char aux_buff[BUFLEN];

	message_udp *msg_udp = (message_udp *)aux_buff;
	msg_udp->cli_addr = cliaddr;
	memcpy(msg_udp->topic, buffer, 50);
	msg_udp->topic[50] = '\0';
	msg_udp->data_type = buffer[50];
	memcpy(msg_udp->content, buffer + 51, nbytes - 51);
	msg_udp->content[1501] = '\n';

	vector<subscriber> subs = subscribed_to_topics[msg_udp->topic];
	short msg_size = get_message_udp_size(msg_udp);
			buffer[msg_size] = '\0';
			memmove(aux_buff + 2, aux_buff, msg_size + 1);
			memcpy(aux_buff, &msg_size, sizeof(short));

	for (int i = 0; i < subs.size(); ++i) {

		char client_id[11];
		memcpy(client_id, subs[i].client_id, sizeof(subs[i].client_id));

		if (connected_ids_fds.find(client_id) != connected_ids_fds.end()) {
			// pregateste mesajul si il trimite la clientul de pe socketul
			// connected_ids_fds[client_id]
			
			int n = send(connected_ids_fds[client_id], aux_buff, msg_size + 2, 0);
			DIE(n < 0, "send");
		} else if (subs[i].sf == 1) { 
			// baga mesajul in buffer
		}
	}
}

void udp_recv_message(int sockfd) {
	char buffer[BUFLEN];
	sockaddr_in cliaddr;
	socklen_t len = sizeof(cliaddr);
	memset(&cliaddr, 0, sizeof(cliaddr));

	int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
					(struct sockaddr *)&cliaddr, &len);
	DIE(n < 0, "recvfrom");

	if (n == 0) return;

	// Add \0 to the end of the received message
	buffer[n] = '\0';
	manage_udp_message(buffer, cliaddr, n);
}

void subscribe(char client_id[], char topic[], int sf) {
	vector<subscriber> subs_topic;
	subscriber subs;
	memcpy(subs.client_id, client_id, strlen(client_id));
	subs.sf = sf;
	if (subscribed_to_topics.find(topic) == subscribed_to_topics.end()) {
		subs_topic.push_back(subs);
		subscribed_to_topics.insert({topic, subs_topic});
	} else {
		subscribed_to_topics[topic].push_back(subs);
	}
}

void unsubscribe(char client_id[], char topic[]) {
	vector<subscriber> &subs_topic = subscribed_to_topics[topic];

	for (int i = 0; i < subs_topic.size(); ++i) {
		if (strcmp(subs_topic[i].client_id, client_id) == 0) {
			subs_topic.erase(subs_topic.begin() + i);
			break;
		}
	}
}

void parse_tcp_msg(char buffer[], char client_id[]) {
	char *ptr, topic[51];
	topic[51] = '\0';
	int sf;
	ptr = strtok(buffer, " \n");
	if (strcmp(ptr, "subscribe") == 0) {
		ptr = strtok(NULL, " ");
		if (!ptr) return;
		memcpy(topic, ptr, strlen(ptr) + 1);

		ptr = strtok(NULL, " \n");
		if (!ptr) return;
		sf = atoi(ptr);

		if (sf < 0 || sf > 1) return;

		subscribe(client_id, topic, sf);
	} else if (strcmp(ptr, "unsubscribe") == 0) {
		ptr = strtok(NULL, " \n");
		if (!ptr) return;
		memcpy(topic, ptr, strlen(ptr) + 1);

		unsubscribe(client_id, topic);
	}
}

void manage_tcp_message(char buffer[], fd_set &read_fds, int index, int nbytes) {

	int sockfd = sock_data_list[index].fd;

	if (umap_subscribers.find(sockfd) == umap_subscribers.end()) {
		subscriber_msg subs;
		memcpy(subs.client_id, buffer + 1, buffer[0]);
		string conn_id(subs.client_id);
		subs.curr_msg_size = -1;

		// se verifica daca mai exista un alt client cu acelasi id
		if (connected_ids_fds.find(conn_id) != connected_ids_fds.end()) {
			printf("Client %s already connected.\n", conn_id.c_str());
			close(sockfd);
			
			// se scoate din multimea de citire socketul inchis 
			FD_CLR(sockfd, &read_fds);
			sock_data_list.erase(sock_data_list.begin() + index);
		} else {
			sock_data client_data = sock_data_list[index];
			printf("New client %s connected from %s:%d.\n",
					subs.client_id, inet_ntoa(client_data.cli_addr.sin_addr), 
					ntohs(client_data.cli_addr.sin_port));
			umap_subscribers.insert({sockfd, subs});
			connected_ids_fds[conn_id] = sockfd;
		}

		nbytes -= buffer[0] + 1;
		buffer += buffer[0] + 1;
	}

	while(nbytes) {
		subscriber_msg &subs = umap_subscribers[sockfd];

		if (subs.curr_msg_size == -1) {
			subs.curr_msg_size = *(short *) buffer;
			subs.copy_msg_size = subs.curr_msg_size;
			buffer += 2;
			nbytes -= 2;
		} else if (nbytes < subs.curr_msg_size) {
			memcpy(subs.buffer + (subs.copy_msg_size - subs.curr_msg_size),
				   buffer, nbytes);
			subs.curr_msg_size -= nbytes;
			nbytes = 0;
		} else {
			memcpy(subs.buffer + (subs.copy_msg_size - subs.curr_msg_size),
				   buffer, subs.curr_msg_size);
			buffer += subs.curr_msg_size;
			nbytes -= subs.curr_msg_size;
			subs.buffer[subs.copy_msg_size] = '\0';
			parse_tcp_msg(subs.buffer, subs.client_id);
			subs.buffer[0] = '\0';
			subs.curr_msg_size = -1;
		}
	}
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int listenfd, connfd, udpfd, fdmax, portnr;
    struct sockaddr_in serv_addr, cli_addr;
    char buffer[BUFLEN];
    int n, ret, exit_while = 0, flag = 1;
    socklen_t cli_len;
    fd_set read_fds, tmp_fds;

	// Socket TCP
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(listenfd < 0, "socket");

	n = setsockopt(listenfd, 
					IPPROTO_TCP,
					TCP_NODELAY,
					&flag,
					sizeof(flag));
	DIE(n < 0, "setsock");

	// portul folosit
	portnr = atoi(argv[1]);
	DIE(portnr == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portnr);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(listenfd, (sockaddr *) &serv_addr, sizeof(sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(listenfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// Creating socket file descriptor
	udpfd = socket(AF_INET, SOCK_DGRAM, 0);

	// Bind the socket with the server address
	int rc = bind(udpfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(rc < 0, "bind failed");

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
 
	// set listenfd in readset
	FD_SET(udpfd, &read_fds);
	FD_SET(listenfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
    fdmax = max(listenfd, udpfd) + 1;

	sock_data_list.push_back({listenfd, serv_addr});
	sock_data_list.push_back({udpfd, serv_addr});

    while(!exit_while) {
		tmp_fds = read_fds;

        // select the ready descriptor
        ret = select(fdmax, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        for (int i = 0; i < sock_data_list.size(); i++) {
			if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
				// se citeste de la tastatura
				memset(buffer, 0, BUFLEN);
				n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
				DIE(n < 0, "recv");
				if (strncmp(buffer, "exit", 4) == 0) {
					exit_while = 1;
					break;
				}
			}

			if (FD_ISSET(sock_data_list[i].fd, &tmp_fds)) {
				if (sock_data_list[i].fd == listenfd) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					cli_len = sizeof(cli_addr);
					connfd = accept(listenfd, (struct sockaddr *) &cli_addr, &cli_len);
					DIE(connfd < 0, "accept");

					n = setsockopt(connfd, 
									IPPROTO_TCP,
									TCP_NODELAY,
									&flag,
									sizeof(flag));
					DIE(n < 0, "setsock");
					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(connfd, &read_fds);
					if (connfd >= fdmax) { 
						fdmax = connfd + 1;
					}

					sock_data data;
					data.fd = connfd;
					data.cli_addr = cli_addr;
					sock_data_list.push_back(data);
				} else if (sock_data_list[i].fd == udpfd) {
					udp_recv_message(udpfd);
				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(sock_data_list[i].fd, buffer, sizeof(buffer) - 1, 0);
					DIE(n < 0, "recv");

					buffer[n] = '\0';
					
					if (n == 0) {
						// conexiunea s-a inchis
						printf("Client %s disconnected.\n", umap_subscribers[sock_data_list[i].fd].client_id);
						close(sock_data_list[i].fd);
						
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(sock_data_list[i].fd, &read_fds);
						connected_ids_fds.erase(umap_subscribers[sock_data_list[i].fd].client_id);
						
						umap_subscribers.erase(sock_data_list[i].fd);
						sock_data_list.erase(sock_data_list.begin() + i);

					} else {
						// printf("Mesajul primit: %s\n", buffer + 2); comm
						manage_tcp_message(buffer, read_fds, i, n);
					}
				}
			}
		}
	}

	for (int i = 0; i < sock_data_list.size(); i++) {
		close(sock_data_list[i].fd);
	}

	return 0;
}
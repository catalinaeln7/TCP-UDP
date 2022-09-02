#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		2000	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	100000	// numarul maxim de clienti in asteptare

struct message_udp {
	sockaddr_in cli_addr;
    char topic[51];
    uint8_t data_type;
	char content[BUFLEN];
}__attribute__((packed));

short get_message_udp_size(message_udp *mess) {
	short len = sizeof(mess->cli_addr)
				+ sizeof(mess->topic)
				+ sizeof(mess->data_type);
	mess->content[1500] = '\0';
	if (mess->data_type == 0) {
		len += 5;
	} else if (mess->data_type == 1) {
		len += 2;
	} else if (mess->data_type == 2) {
		len += 6;
	} else if (mess->data_type == 3) {
		len+= strlen(mess->content);
	}

	return len;
}

#endif

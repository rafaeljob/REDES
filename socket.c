#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#define _BSD_SOURCE
#include <endian.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>


#include "sock_if.h"

static
int
end_of_exec(const char *buf)
{
	return (!strcmp(buf, TERMINATE_STRING));
}

static
void
fill_server_info(struct sockaddr_in *sa_server)
{
	sa_server->sin_family = AF_INET;
	sa_server->sin_port = htons(SOCK_PORT);
	/* use inet_addr or friends otherwise */
	sa_server->sin_addr.s_addr = INADDR_ANY;
}

static
void
client_loop(int sock)
{
	ssize_t ssret;
	char *cret;
	struct sockaddr_in sa_server;
	struct pkt *pkt;

	pkt = calloc(1, sizeof(*pkt));
	if (pkt == NULL)
		err(1, "malloc");	

	fill_server_info(&sa_server);
	while (!end_of_exec(pkt->buf)) {
		printf("input msg >");
		cret = fgets(pkt->buf, BUFSZ, stdin);
		if (cret == NULL)
			err(1, "fgets");
		pkt->buf_len = htobe64(strlen(pkt->buf)); // transforma em big-endian

		ssret = sendto(sock, pkt, sizeof(*pkt), 0, 
				(const struct sockaddr *)&sa_server, 
				sizeof(sa_server));	// MSG_CONFIRM
		if (ssret < 0)
			err(1, "sendto");
	}
	free(pkt);

}

static
void
server_loop(int sock)
{
	int ret;
	ssize_t ssret;
	struct sockaddr_in sa_server;
	struct sockaddr_in sa_client;
	socklen_t	sa_client_len;
	struct pkt *pkt;
	

	fill_server_info(&sa_server);
	ret = bind(sock, (const struct sockaddr *)&sa_server,
	    sizeof(sa_server)); 
	if (ret < 0)
		err(1, "bind");

	printf("bound to port %hu\n", SOCK_PORT);
	pkt = malloc(sizeof(*pkt));
	if (pkt == NULL)
		err(1, "malloc");
	/* make sure its not end-of-exec string */
	pkt->buf[0] = '\0';
	while (!end_of_exec(pkt->buf)) {
		bzero(&sa_client, sizeof(sa_client));
		bzero(pkt, sizeof(*pkt));

		ssret = recvfrom(sock, pkt, sizeof(*pkt), 0,
				(struct sockaddr *)&sa_client, &sa_client_len);
		if (ssret < 0)
			err(1, "recvfrom");

		pkt->buf_len = be64toh(pkt->buf_len);
		if (pkt->buf_len > BUFSZ) {
			printf("malformed packet. buf_len %zu\n", pkt->buf_len);
			continue;
		}

		if (pkt->buf[pkt->buf_len]) {
			printf("expected zero-terminated string!\n");
			pkt->buf[pkt->buf_len] = '\0';
		}
		printf("%s\n", pkt->buf);
	}
	free(pkt);
}

int
main(int argc, char **argv)
{
	int sock;
	int c;
	int choice;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	//af_inet = ipv4 / sock_dgram = datagramas com fixed size length / 0 = protocol
	if (sock < 0)
		err(1, "socket");

	printf("socket %d\n", sock);

	choice = 0;
	while ((c = getopt(argc, argv, "cs")) != -1) {
		switch (c) {
		case 'c':
			choice = 1;
			break;
		case 's':
			choice = 2;
			break;
		default:
			choice = 0;
			break;
		}
	}

	if (choice == 1)
		client_loop(sock);
	else if (choice == 2)
		server_loop(sock);
	else
		printf("wrong choice\n");

	c = close(sock);
	if (c < 0)
		err(1, "close");
	return (0);
}

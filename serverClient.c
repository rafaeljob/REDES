//---------------------------------------------
//	Includes
//---------------------------------------------

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <endian.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

//---------------------------------------------
//	Defines
//---------------------------------------------

#define IP_SERVER INADDR_ANY
#define IP_CLIENT INADDR_ANY

#define POL_CRC 0x14B //101001011 -- x8 + x6 + x3 + x1 + 1
#define WINDOW_SIZE
#define DATA_SIZE 240

//---------------------------------------------
//	Structs
//---------------------------------------------

struct pkt_data {

	uint32_t ipd;	
	uint32_t ipo;

	uint8_t type;
	uint8_t sequence;
	uint16_t size;
	uint32_t crc;
	uint8_t data[DATA_SIZE];

	//uint64_t length;
	
} __attribute__ ((packed));

struct pkt_cnx {

	uint32_t flux;
	uint8_t size_window;
	uint32_t size_arq;
	
} __attribute__ ((packed));

//---------------------------------------------
//	Client
//---------------------------------------------
/*static
void
fill_server_info(struct sockaddr_in *sa_server)
{
	sa_server->sin_family = AF_INET;
	sa_server->sin_port = htons(SOCK_PORT);
	/* use inet_addr or friends otherwise */
	/*sa_server->sin_addr.s_addr = INADDR_ANY;
}*/


void client(int sock) {

	//ssize_t ssret;
	//char *cret;
	//struct sockaddr_in sa_server;
	struct pkt_data *pkt_d;
	struct pkt_cnx *pkt_c;

	pkt_d = calloc(1, sizeof(*pkt_d));
	if (pkt_d == NULL)
		err(1, "malloc");
	printf("TAM PKT_DATA - %d\n", sizeof(*pkt_d));
	pkt_c = calloc(1, sizeof(*pkt_c));
	if (pkt_c == NULL)
		err(1, "malloc");
	printf("TAM PKT_CNX - %d\n", sizeof(*pkt_c));	
/*
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
	}*/
	free(pkt_d);
	free(pkt_c);
}

//---------------------------------------------
//	Server
//---------------------------------------------

//---------------------------------------------
//	Main
//---------------------------------------------

int main() {
	client(1);
	return 0;
}

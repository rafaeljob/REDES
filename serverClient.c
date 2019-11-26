//	HEADER
//	NOMES: Rafael Basso, Eric Friederich, Gabriel Radzki
//	gcc serverClient.c -o sc
//	./sc -c
//	./sc -s 
///////////////////////////////////////////////

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

#include "crc.h"

//---------------------------------------------
//	Defines
//---------------------------------------------

#define IP_SERVER INADDR_ANY
#define IP_CLIENT INADDR_ANY
#define SOCK_PORT 8888

#define POL_CRC 0x14B //101001011 -- x8 + x6 + x3 + x1 + 1
#define WINDOW_SIZE
#define DATA_SIZE 240
#define TERMINATE_STRING "@exit\n"
#define ASCII_ESC 27

#define DEBUG


//---------------------------------------------
//	Structs
//---------------------------------------------

struct pkt_data {

	uint32_t ipd;	
	uint32_t ipo;

	uint8_t type;
	uint8_t sequence;
	uint16_t len;
	uint32_t crc;
	uint8_t data[DATA_SIZE];
	
} __attribute__ ((packed));

struct pkt_cnx {

	uint32_t ipd;	
	uint32_t ipo;

	uint8_t type;
	uint8_t sequence;
	uint16_t len;
	uint32_t crc;

	uint32_t flux;
	uint8_t size_window;
	uint32_t size_arq;
	
} __attribute__ ((packed));

struct pkt_ack {
	uint32_t ipd;	
	uint32_t ipo;

	uint8_t type;
	uint8_t sequence;

} __attribute__ ((packed));

struct window_queue {
	struct pkt_data *self;
	struct window_queue *next;;
} __attribute__ ((packed));

//---------------------------------------------
//	Global var
//---------------------------------------------

struct window_queue *wq_head = NULL;
struct window_queue *wq_tail = NULL;
int window_size;

//---------------------------------------------
//	Queue
//---------------------------------------------

void append(struct window_queue *wq_e) {
	if(wq_head == NULL) {
		wq_head = wq_e;
		wq_tail = wq_e;	
	} else {
		wq_tail->next = wq_e;
		wq_tail = wq_e;
	}
}

void pop() {
	struct window_queue *temp = wq_head;
	if(wq_head->next != NULL) {
		wq_head = wq_head->next;		
	} else {
		wq_head = NULL;
		wq_tail = NULL;
	}
	temp->next = NULL;
	free(temp->self);
	free(temp);
}

//---------------------------------------------
//	Print
//---------------------------------------------

void print_pkt_connection(struct pkt_cnx *pkt_c) {
	printf("PKT C: \n");
	printf("IP SERVER: %d\n", pkt_c->ipd);
	printf("IP CLIENT: %d\n", pkt_c->ipo);
	printf("TYPE: %d\n", pkt_c->type);
	printf("SEQ: %d\n", pkt_c->sequence);
	printf("LENGTH: %d\n", pkt_c->len);
	printf("CRC: %d\n", pkt_c->crc);
	printf("FLUX: %d\n", pkt_c->flux);
	printf("WINDOW SIZE: %d\n", pkt_c->size_window);
	printf("FILE SIZE: %d\n", pkt_c->size_arq);
}

void print_pkt(struct pkt_data *pkt_d) {
	printf("PKT D: \n");
	printf("IP SERVER: %d\n", pkt_d->ipd);
	printf("IP CLIENT: %d\n", pkt_d->ipo);
	printf("TYPE: %d\n", pkt_d->type);
	printf("SEQ: %d\n", pkt_d->sequence);
	printf("LENGTH: %d\n", pkt_d->len);
	printf("CRC: %d\n", pkt_d->crc);
	printf("DATA: %s\n", pkt_d->data);
}

void print_queue(struct window_queue *wq_top) {
	if(wq_top != NULL) {
		printf("ADD: %x\n", wq_top);
		printf("ADD NEXT: %x\n", wq_top->next);
		print_pkt(wq_top->self);
		printf("sizeof--%ld\n", sizeof(*wq_top->self));
		printf("----------------------------------------------------------------------------------\n");
		print_queue(wq_top->next);
	}
}

void print() {
	printf("\033[1;33m");
	printf("REDES@input");
	printf("\033[0m");
	printf(": -> ");
}

void print_clear() {
	//^[[2K
	printf("\033[2K", ASCII_ESC);

}

//---------------------------------------------
//	Big-Little Endian
//---------------------------------------------

void betoh_data(struct pkt_data *pkt) {
	pkt->ipd = be32toh(pkt->ipd);	
	pkt->ipo = be32toh(pkt->ipo);
	pkt->len = be16toh(pkt->len);
	pkt->crc = be32toh(pkt->crc);
}

void htobe_data(struct pkt_data *pkt) {
	pkt->ipd = htobe32(pkt->ipd);	
	pkt->ipo = htobe32(pkt->ipo);
	pkt->len = htobe16(pkt->len);
	pkt->crc = htobe32(pkt->crc);
}

void betoh_conn(struct pkt_cnx *pkt) {
	pkt->ipd = be32toh(pkt->ipd);	
	pkt->ipo = be32toh(pkt->ipo);
	pkt->len = be16toh(pkt->len);
	pkt->crc = be32toh(pkt->crc);
	pkt->flux = be32toh(pkt->flux);
	pkt->size_arq = be32toh(pkt->size_arq);
}

void htobe_conn(struct pkt_cnx *pkt) {
	pkt->ipd = htobe32(pkt->ipd);	
	pkt->ipo = htobe32(pkt->ipo);
	pkt->len = htobe16(pkt->len);
	pkt->crc = htobe32(pkt->crc);
	pkt->flux = htobe32(pkt->flux);
	pkt->size_arq = htobe32(pkt->size_arq);
}

void betoh_ack(struct pkt_ack *pkt) {
	pkt->ipd = be32toh(pkt->ipd);	
	pkt->ipo = be32toh(pkt->ipo);
}

void htobe_ack(struct pkt_ack *pkt) {
	pkt->ipd = htobe32(pkt->ipd);	
	pkt->ipo = htobe32(pkt->ipo);
}

//---------------------------------------------
//	Client
//---------------------------------------------

void fill_server_info(struct sockaddr_in *sa_server, char *ip_buffer) {
	sa_server->sin_family = AF_INET;
	sa_server->sin_port = htons(SOCK_PORT);
	sa_server->sin_addr.s_addr = inet_addr(ip_buffer);
}

void fill_pkt_connection_info(struct pkt_cnx *pkt_c, int flux, int file_size, char *ip_buffer) {
	pkt_c->ipd = inet_addr(ip_buffer);
	pkt_c->ipo = IP_CLIENT;
	pkt_c->type = 2;
	pkt_c->sequence = 0;
	pkt_c->len = 9;
	pkt_c->crc = 0;

	pkt_c->flux = flux;
	pkt_c->size_window = window_size;
	pkt_c->size_arq = file_size;

	//crc(*pkt_c);
}

int fill_vet_addr(struct window_queue **vet) {
	struct window_queue *wq_e = wq_head;
	int ret;

	for(int i = 0; i < window_size; i++) {
		if(wq_e != NULL) {
			vet[i] = wq_e;
			wq_e = wq_e->next;
			ret = i;
		} else { vet[i] = NULL;}
		printf("addr %d: %x\n", i, vet[i]);
	}
	return ret + 1;
} 

void pop_n(int n) {
	for(int i = 0; i < n; i++) {
		printf("popping data from queue\n");	
		pop();
	}
}

void go_back_n_client(int sock, struct sockaddr_in sa_server) {
	struct sockaddr_in sa_client;
	struct pkt_ack *pkt_a;
	struct window_queue *vet_addr[window_size];
	socklen_t sa_client_len = sizeof(sa_client);
	ssize_t ssret;
	int npop;

	while(wq_head != NULL) {
		npop = fill_vet_addr(&vet_addr);

		pkt_a = malloc(sizeof(*pkt_a));
		if (pkt_a == NULL) { err(1, "malloc ack");}
		
		bzero(&sa_client, sizeof(sa_client));
		bzero(pkt_a, sizeof(*pkt_a));
		
		for(int i = 0; i < npop; i++) {
			htobe_data(vet_addr[i]->self);
			printf("send %d\n", i);
			
			ssret = sendto(sock, vet_addr[i]->self, sizeof(*vet_addr[i]->self), 0, (const struct sockaddr *)&sa_server, sizeof(sa_server));
			if(ssret < 0) { err(1, "sendto");}
			usleep(80000);
		}

		printf("waiting ack\n");
		ssret = recvfrom(sock, pkt_a, sizeof(*pkt_a), 0, (struct sockaddr *)&sa_client, &sa_client_len);
		if(ssret < 0) { err(1, "recvfrom");}

		betoh_ack(pkt_a);

		while(pkt_a->sequence != window_size) {
			printf("retransmitting\n");
			for(int i = pkt_a->sequence; i < npop; i++) {
				printf("resend %d\n", i);
			
				ssret = sendto(sock, vet_addr[i]->self, sizeof(*vet_addr[i]->self), 0, (const struct sockaddr *)&sa_server, sizeof(sa_server));
				if(ssret < 0) { err(1, "sendto");}
				usleep(80000);
		
			}

			bzero(&sa_client, sizeof(sa_client));
			bzero(pkt_a, sizeof(*pkt_a));
				
			ssret = recvfrom(sock, pkt_a, sizeof(*pkt_a), 0, (struct sockaddr *)&sa_client, &sa_client_len);
			if(ssret < 0) { err(1, "recvfrom");}

			betoh_ack(pkt_a);
		}
		free(pkt_a);
		pop_n(npop);
	}	
}

void stop_n_wait_client(int sock, struct sockaddr_in sa_server) {
	struct sockaddr_in sa_client;
	struct pkt_ack *pkt_a;
	socklen_t sa_client_len;
	ssize_t ssret;

	crcInit();
	while(wq_head != NULL) {
		pkt_a = malloc(sizeof(*pkt_a));
		if (pkt_a == NULL) { err(1, "malloc ack");}

		bzero(&sa_client, sizeof(sa_client));
		bzero(pkt_a, sizeof(*pkt_a));
		
		htobe_data(wq_head->self);
		printf("send\n");

		ssret = sendto(sock, wq_head->self, sizeof(*wq_head->self), 0, (const struct sockaddr *)&sa_server, sizeof(sa_server));
		if(ssret < 0) { err(1, "sendto");}
		
		printf("waiting ack\n");
		ssret = recvfrom(sock, pkt_a, sizeof(*pkt_a), 0, (struct sockaddr *)&sa_client, &sa_client_len);
		if(ssret < 0) { err(1, "recvfrom");}

		//big endian funcs
		betoh_ack(pkt_a);

		while(pkt_a->sequence == wq_head->self->sequence) {
			printf("retransmitting\n");
			ssret = sendto(sock, wq_head->self, sizeof(*wq_head->self), 0, (const struct sockaddr *)&sa_server, sizeof(sa_server));
			if(ssret < 0) { err(1, "sendto");}
			
			bzero(&sa_client, sizeof(sa_client));
			bzero(pkt_a, sizeof(*pkt_a));
				
			ssret = recvfrom(sock, pkt_a, sizeof(*pkt_a), 0, (struct sockaddr *)&sa_client, &sa_client_len);
			if(ssret < 0) { err(1, "recvfrom");}

			betoh_ack(pkt_a);
		}
		
		printf("poping data from queue -- data sent\n");
		pop();
		free(pkt_a);
	}
}

void send_file(int sock) {
	struct pkt_cnx *pkt_c;
	struct sockaddr_in sa_server;
	unsigned char ip_buffer[16];
	int size, tec;
	ssize_t ssret;
	
	pkt_c = calloc(1, sizeof(*pkt_c));
	if(pkt_c == NULL) { err(1, "malloc");}

	printf("stop-and-wait(0) or go-back-n(1)\n");
	print();
	scanf("%d", &tec);
	if(tec == 0) {
		window_size = 1;
	} else {
		printf("window size:\n");
		print();
		scanf("%d", &window_size);
	}

	size = read_file();
	
	printf("type the destination ip address \n");
	print();
	scanf("%s", ip_buffer);

	fill_pkt_connection_info(pkt_c, tec, size, ip_buffer);
	htobe_conn(pkt_c);

	fill_server_info(&sa_server, ip_buffer);
	
	ssret = sendto(sock, pkt_c, sizeof(*pkt_c), 0, (const struct sockaddr *)&sa_server, sizeof(sa_server));
	if(ssret < 0) { err(1, "sendto");}
	//envia pkt conexao

	switch(tec) {
		case 0:
			stop_n_wait_client(sock, sa_server);
			break;
		case 1:
			sleep(2);
			go_back_n_client(sock, sa_server);
			break;
		default:
			printf("error\n");
			break;
	}
}



int read_file() {
	struct window_queue *wq_e;
	struct pkt_data *pkt_d;
	unsigned char buf[DATA_SIZE];
	char file[20];
	char *cret;
	size_t fsz;
	int file_sz = 0;
	int i = 0;
	FILE *fp;	
	
	printf("type the file name:\n");	
	print();
	scanf("%s", file);

	fp = fopen(file, "r");
	if(fp < 0) { err(1, "fopen");}	

	printf("reading file (%s)\n", file);
	crcInit();
	while(1) {
		fsz = fread(buf, sizeof(char), DATA_SIZE, (FILE*)fp);
		
		wq_e = calloc(1, sizeof(*wq_e));
		if(wq_e == NULL) { err(1, "malloc");}		

		pkt_d = calloc(1, sizeof(*pkt_d));
		if(pkt_d == NULL) { err(1, "malloc");}
	
		cret = memcpy(pkt_d->data, buf, fsz);
		if(cret == NULL) { err(1, "memcpy");}
		
		pkt_d->sequence = i;	
		pkt_d->len = fsz;

		pkt_d->crc = crcFast((char*)pkt_d->data, pkt_d->len);

		//printf("CRC32: %X\n", pkt_d->crc);
	
		wq_e->self = pkt_d;
		wq_e->next = NULL;
		append(wq_e);
		
		file_sz = file_sz + fsz;
		bzero(buf, DATA_SIZE);	//zera buf

		if(i < window_size - 1) { i++;}
		else {i = 0;}
		if(fsz != DATA_SIZE) { break;}
	}

	fclose(fp);
	return file_sz;
}

void client(int sock) {

	int opt;
	printf("starting client\n\n");
	while(1) {
		printf("Send: string(1) or file(2)\n");
		print();
		scanf("%d", &opt);
		if(opt == 1) {
			printf("string\n");
			//send_string(sock, flux, n);
		} else if(opt == 2) {
			printf("file\n");
			send_file(sock);
		} else { printf("try 1 or 2\n");}
	}	
}

//---------------------------------------------
//	Server
//---------------------------------------------

void fill_pkt_ack_info(struct pkt_ack *pkt_a, int seq) {
	pkt_a->ipd = INADDR_ANY; //inet_addr(ip_buffer);
	pkt_a->ipo = IP_CLIENT;
	pkt_a->type = 1;
	pkt_a->sequence = seq;
}

void server(int sock) {
	struct sockaddr_in sa_server;
	struct sockaddr_in sa_client;
	struct pkt_cnx *pkt_c;
	socklen_t sa_client_len = sizeof(sa_client);
	unsigned char ip_buffer[16];
	ssize_t ssret;
	int ret, op, fsz;

	printf("type the server ip\n");
	print();
	scanf("%s", ip_buffer);
	
	fill_server_info(&sa_server, ip_buffer);
	ret = bind(sock, (const struct sockaddr *)&sa_server, sizeof(sa_server)); 
	if (ret < 0) { err(1, "bind");}

	printf("bound to port %hu\n", SOCK_PORT);
	pkt_c = malloc(sizeof(*pkt_c));
	if (pkt_c == NULL) { err(1, "malloc");}

	while(1) {
		printf("waiting connection packet...\n");
		bzero(&sa_client, sizeof(sa_client));
		bzero(pkt_c, sizeof(*pkt_c));

		ssret = recvfrom(sock, pkt_c, sizeof(*pkt_c), 0, (struct sockaddr *)&sa_client, &sa_client_len);
		if(ssret < 0) { err(1, "recvfrom cnx");}

		betoh_conn(pkt_c);

		printf("packet received\n");
		if(pkt_c->type != 2) {
			printf("different packet expected\n");
		} else {
			printf("connection packet received\n");

			//set infos do pkt
			fsz = pkt_c->size_arq;
			window_size = pkt_c->size_window;
			op = pkt_c->flux;
			free(pkt_c);

			switch(op) {
				case 0:
					printf("*stop and wait selected\n");
					printf("*window size: %d\n", window_size);
					printf("*arq size: %d bytes\n", fsz);

					stop_n_wait_server(sock, sa_server);
					ret = write_file(fsz);

					if(ret != 1) { err(1, "write_file");}
					printf("*file wrote\n");

					break;
				case 1:
					printf("*go back n selected\n");
					printf("*window size: %d\n", window_size);
					printf("*arq size: %d bytes\n", fsz);

					go_back_n_server(sock, sa_server);
					ret = write_file(fsz);

					if(ret != 1) { err(1, "write_file");}
					printf("*file wrote\n");
					break;
				default:
					printf("no opt!\n");
					break;
			}
		}
		
	}
	
}

void stop_n_wait_server(int sock, struct sockaddr_in sa_server) {
	struct sockaddr_in sa_client;
	struct window_queue *wq_e;
	uint32_t checksum;
	struct pkt_data *pkt_d;
	struct pkt_ack *pkt_a;
	socklen_t sa_client_len = sizeof(sa_client);
	ssize_t ssret;
	int sz = DATA_SIZE;
	int seq;

	crcInit(); 
	
	int pkt_count = 0;

	while(sz == DATA_SIZE) {

		wq_e = calloc(1, sizeof(*wq_e));
		if(wq_e == NULL) { err(1, "malloc queue");}

		pkt_d = malloc(sizeof(*pkt_d));
		if (pkt_d == NULL) { err(1, "malloc data");}

		pkt_a = malloc(sizeof(*pkt_a));
		if (pkt_a == NULL) { err(1, "malloc ack");}

		bzero(&sa_client, sizeof(sa_client));
		bzero(pkt_d, sizeof(*pkt_d));
		
		ssret = recvfrom(sock, pkt_d, sizeof(*pkt_d), 0, (struct sockaddr *)&sa_client, &sa_client_len);
		if(ssret < 0) { err(1, "recvfrom");}

		//big endian funcs
		betoh_data(pkt_d);

		sz = pkt_d->len;

		// Inserts error if packet_count is a multiple of ten
		if (pkt_count % 10 == 0){ 
			pkt_d->data[10] ^= 1UL << 4;
		}

		checksum = crcFast((char*)pkt_d->data, pkt_d->len);

		#ifdef DEBUG

			printf("RECEIVED CRC: %X\n", pkt_d->crc);
			printf("CALCULATED CRC: %X\n", checksum);

			if (checksum == pkt_d->crc) {
				printf("CRC Matches.\n");
			} else {
				printf("CRC Mismatch.\n");
			}

		#endif

		//teste erros
		if(checksum == pkt_d->crc) {
			seq = 1;
			wq_e->self = pkt_d;
			wq_e->next = NULL;
			append(wq_e);
		} else { 
			seq = 0;
		}
		
		fill_pkt_ack_info(pkt_a, seq);
		htobe_ack(pkt_a);

		ssret = sendto(sock, pkt_a, sizeof(*pkt_a), 0, (const struct sockaddr *)&sa_client, sa_client_len);
		if(ssret < 0) { err(1, "sendto");}
		
		free(pkt_a);

		pkt_count++;
	}

}

void go_back_n_server(int sock, struct sockaddr_in sa_server) {
	struct sockaddr_in sa_client;
	uint32_t checksum;
	struct window_queue *wq_e; 
	struct pkt_data *pkt_d;
	struct pkt_ack *pkt_a;
	socklen_t sa_client_len = sizeof(sa_client);
	ssize_t ssret;
	int sz = DATA_SIZE;
	int sq_e;
	int erro = 0;

	int pkt_count = 0;

	crcInit();
	while(sz == DATA_SIZE) {

		pkt_a = malloc(sizeof(*pkt_a));
		if(pkt_a == NULL) { err(1, "malloc ack");}

		wq_e = calloc(1, sizeof(*wq_e));
		if(wq_e == NULL) { err(1, "malloc queue");}

		pkt_d = malloc(sizeof(*pkt_d));
		if(pkt_d == NULL) { err(1, "malloc data");}

		bzero(&sa_client, sizeof(sa_client));
		bzero(pkt_d, sizeof(*pkt_d));
		
		ssret = recvfrom(sock, pkt_d, sizeof(*pkt_d), 0, (struct sockaddr *)&sa_client, &sa_client_len);
		if(ssret < 0) { err(1, "recvfrom");}

		//big endian funcs
		betoh_data(pkt_d);

		sz = pkt_d->len;

		// Inserts error if packet_count is a multiple of ten
		if (pkt_count % 10 == 0){ 
			pkt_d->data[10] ^= 1UL << 4;
		}

		checksum = crcFast((char*)pkt_d->data, pkt_d->len);

		#ifdef DEBUG

			printf("RECEIVED CRC: %X\n", pkt_d->crc);
			printf("CALCULATED CRC: %X\n", checksum);

			if (checksum == pkt_d->crc) {
				printf("CRC Matches.\n");
			} else {
				printf("CRC Mismatch.\n");
			}

		#endif

		//teste erros
		if(checksum == pkt_d->crc && erro == 0) {
		
			wq_e->self = pkt_d;
			wq_e->next = NULL;
			append(wq_e);
		
		} else if(erro == 0){
			
			free(pkt_d);
			free(wq_e);
			sq_e = pkt_d->sequence;
			erro = 1;	
		
		} else {
			
			free(pkt_d);
			free(wq_e);
		
		}

		if(pkt_d->sequence = window_size - 1) {	
			if(erro == 0) {	
				fill_pkt_ack_info(pkt_a, pkt_d->sequence + 1);
			} else {
				fill_pkt_ack_info(pkt_a, sq_e);
			}
			
			htobe_ack(pkt_a);

			ssret = sendto(sock, pkt_a, sizeof(*pkt_a), 0, (const struct sockaddr *)&sa_client, sa_client_len);
			if(ssret < 0) { err(1, "sendto");}
			erro = 0;
		}

		free(pkt_a);
		pkt_count++;
	}
	
}

int write_file(int size) {
	unsigned char buffer[20];
	size_t fsz;
	int file_sz = 0;
	FILE *fp;	

	printf("type the file name\n");
	print();
	scanf("%s", buffer);

	fp = fopen(buffer, "w");
	if(fp < 0) { err(1, "fopen");}	

	printf("writing received file (%s)\n", buffer);
	while(wq_head != NULL) {
		fsz = fwrite(wq_head->self->data, sizeof(char), wq_head->self->len, (FILE*)fp);
		file_sz = file_sz + fsz;
		pop();
	}

	fclose(fp);

	if((size - file_sz) == 0) { return 1;}	
	return 0;
}

//---------------------------------------------
//	Main
//---------------------------------------------

int main(int argc, char **argv) {

	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	//af_inet = ipv4 / sock_dgram = datagramas com fixed size length / 0 = protocol
	if(sock < 0) { err(1, "socket");}
	printf("socket %d\n", sock);

	char c = getopt(argc, argv, "cs");
	switch(c) {
		case 'c':
			client(sock);
			break;
		case 's':
			server(sock);
			break;
		default:
			printf("no opt!\n");
			break;
	}
	
	int ret = close(sock);
	if(ret < 0) { err(1, "close");}

	return 0;
}

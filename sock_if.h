#ifndef __SOCK_IF_H__
#define __SOCK_IF_H__

#define SOCK_PORT	9876
#define BUFSZ		2048

#define TERMINATE_STRING	"q\n"

struct pkt {
	uint64_t buf_len;
	char buf[BUFSZ];
};
#endif

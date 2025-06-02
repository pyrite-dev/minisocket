#define MINISOCKET_IMPLEMENTATION
#include "minisocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

int main(int argc, char** argv) {
	ms_interface_t* net;

	if(argc != 2) {
		fprintf(stderr, "Usage: %s host\n", argv[0]);
		return 1;
	}
	net = ms_tcp(argv[1], 80);
	if(net != NULL) {
		char*  buffer = malloc(1);
		size_t sz     = 0;
		char req[1024];
		ms_buffer_t* buf;
		buffer[0]     = 0;

		req[0] = 0;
		strcat(req, "GET / HTTP/1.0\r\nHost: ");
		strcat(req, argv[1]);
		strcat(req, "\r\nConnection: close\r\n\r\n");
		buf = ms_wbuffer(net, strlen(req));
		memcpy(buf->data, req, buf->size);

		while(ms_step(net) == 0) {
			int state = net->state;
			if(state == MS_STATE_AFTER_WRITE || state == MS_STATE_AFTER_READ) {
				buf = ms_rbuffer(net, 512);
			} else if(state == MS_STATE_READ_COMPLETE || state == MS_STATE_READ_PART) {
				char* old = buffer;
				buffer	  = malloc(sz + buf->size + 1);
				memcpy(buffer, old, sz);
				memcpy(buffer + sz, buf->data, buf->size);
				buffer[sz + buf->size] = 0;
				sz += buf->size;
				free(old);
			}
		}

		ms_destroy(net);

		printf("%s\n", buffer);
		free(buffer);
	}
}

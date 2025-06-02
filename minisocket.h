#ifndef __MINISOCKET_H__
#define __MINISOCKET_H__

/**
 * To use this library - do this in one C or C++ file:
 *   #define MINISOCKET_IMPLEMENTATION
 *   #include "minisocket.h"
 */

#ifndef MSDEF
#ifdef MS_STATIC
#define MSDEF static
#else
#define MSDEF extern
#endif
#endif

#ifndef MS_TIMEOUT
#define MS_TIMEOUT 3
#endif

#ifndef MS_HAS_C99
#ifdef __STDC_VERSION__
#if __STDC_VERSION__ >= 199901L
#define MS_HAS_C99
#endif
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <time.h>

#ifdef MS_HAS_C99
#include <stdint.h>
typedef uint32_t ms_u32;
#else
typedef unsigned int ms_u32;
#endif

/**
 * @~english
 * @brief Network states
 */
enum MS_STATES {
	MS_STATE_PRE_CONNECT = 0,
	MS_STATE_CONNECT,
	MS_STATE_CONNECTED,
	MS_STATE_WRITE,
	MS_STATE_READ,
	MS_STATE_WRITE_COMPLETE,
	MS_STATE_READ_COMPLETE,
	MS_STATE_WRITE_PART,
	MS_STATE_READ_PART,
	MS_STATE_AFTER_WRITE,
	MS_STATE_AFTER_READ,
	MS_STATE_FAILED,
	MS_STATE_FAILED_CONNECT,
	MS_STATE_FAILED_SOCKET,
	MS_STATE_FAILED_WRITE,
	MS_STATE_FAILED_READ,
};

/**
 * @struct ms_buffer_t
 * @~english
 * @brief Network buffer
 *
 * @var ms_buffer_t::data
 * @brief Data
 *
 * @var ms_buffer_t::size
 * @brief Size of data
 *
 * @var ms_buffer_t::_size
 * @brief Size of data
 * @note This is used internally - you probably want to read ms_buffer::size instead
 *
 * @var ms_buffer_t::seek
 * @brief Seek position
 */
typedef struct ms_buffer {
	void*  data;
	size_t size;
	size_t _size;
	int    seek;
} ms_buffer_t;

/**
 * @struct ms_interface_t
 * @~english
 * @brief Network interface
 *
 * @var ms_interface_t::engine
 * @brief Engine instance
 *
 * @var ms_interface_t::port
 * @brief Port
 *
 * @var ms_interface_t::sock
 * @brief Socket
 *
 * @var ms_interface_t::state
 * @brief State
 *
 * @var ms_interface_t::tcp
 * @brief `0` if UDP, otherwise TCP.
 *
 * @var ms_interface_t::wqueue
 * @brief Write buffer queue
 *
 * @var ms_interface_t::rqueue
 * @brief Read buffer queue
 *
 * @var ms_interface_t::address
 * @brief List of address to try
 *
 * @var ms_interface_t::index
 * @brief Index for address list
 *
 * @var ms_interface_t::last
 * @brief Time when it issued connect
 *
 * @var ms_interface_t::length
 * @brief Length for address list
 */
typedef struct ms_interface {
	int	      port;
	int	      sock;
	int	      state;
	int	      tcp;
	ms_buffer_t** wqueue;
	ms_buffer_t** rqueue;

	ms_u32* address;
	int	index;
	time_t	last;
	int	length;
} ms_interface_t;

/**
 * @~english
 * @brief Get last network error
 * @return Network error code
 */
MSDEF int ms_get_error(void);

/**
 * @~english
 * @brief Get network error string
 * @return Network error string
 */
MSDEF char* ms_error(int code);

/**
 * @~english
 * @brief Do network single step
 * @param net Network interface
 * @return `0` if successful
 */
MSDEF int ms_step(ms_interface_t* net);

/**
 * @~english
 * @brief Create new write buffer
 * @param net Network interface
 * @param size Size
 * @return Network buffer
 */
MSDEF ms_buffer_t* ms_wbuffer(ms_interface_t* net, size_t size);

/**
 * @~english
 * @brief Create new read buffer
 * @param net Network interface
 * @param size Size
 * @return Network buffer
 */
MSDEF ms_buffer_t* ms_rbuffer(ms_interface_t* net, size_t size);

/**
 * @~english
 * @brief Destroy network interface
 * @param net Network interface
 */
MSDEF void ms_destroy(ms_interface_t* net);

/**
 * @~english
 * @brief Connect to host using TCP/IP
 * @param host Host name
 * @param port Port
 * @return Network interface
 */
MSDEF ms_interface_t* ms_tcp(const char* host, int port);

/**
 * @~english
 * @brief Create socket
 * @param type Socket type, `udp` or `tcp`
 * @return Socket
 */
MSDEF int ms_socket(const char* type);

#ifdef MINISOCKET_IMPLEMENTATION

#ifdef _WIN32
#include <winsock.h>
#include <windows.h>

#define _MS_EINPROGRESS WSAEINPROGRESS
#define _MS_EINTR WSAEINTR
#define _MS_EWOULDBLOCK WSAEWOULDBLOCK
#else
#ifdef __OS2__
#include <types.h>
#include <sys/time.h>
#endif

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netdb.h>

#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#define _MS_EINPROGRESS EINPROGRESS
#define _MS_EINTR EINTR
#define _MS_EWOULDBLOCK EWOULDBLOCK
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define _MS_IS_FINE_ERROR(x) ((x) == _MS_EWOULDBLOCK || (x) == _MS_EINPROGRESS)

static void ms_non_block(int sock) {
	/* TODO: what is difference between this and fcntl O_NONBLOCK/O_NDELAY? */
#ifdef _WIN32
	u_long val = 1;
	ioctlsocket(sock, FIONBIO, &val);
#else
	int val = 1;
	ioctl(sock, FIONBIO, &val);
#endif
}

static void ms_close(int sock) {
#ifdef _WIN32
	closesocket(sock);
#else
	close(sock);
#endif
}

static ms_interface_t* ms_init(const char* host, int port) {
	struct hostent* h;
	int		i;
#ifdef _WIN32
	int s;
#endif
	ms_interface_t* r = malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));

#ifdef _WIN32
	s = ms_socket("tcp");
	if(s == -1 && ms_get_error() == WSANOTINITIALISED) {
		WSADATA wsa;
		WSAStartup(MAKEWORD(1, 1), &wsa);
	} else {
		ms_close(s);
	}
#endif

	h = gethostbyname(host);
	if(h == NULL || h->h_addrtype != AF_INET) {
		free(r);
		return NULL;
	}

	r->sock = -1;

	r->state = MS_STATE_PRE_CONNECT;
	r->port	 = port;

	r->index = 0;

	for(i = 0; h->h_addr_list[i] != NULL; i++);
	r->address = malloc(sizeof(*r->address) * i);
	r->length  = i;

	for(i = 0; h->h_addr_list[i] != NULL; i++) {
		ms_u32 addr   = *(ms_u32*)h->h_addr_list[i];
		r->address[i] = addr;
	}

	r->wqueue = NULL;
	r->rqueue = NULL;

	return r;
}

static int ms_buffer_length(ms_buffer_t*** list) {
	int i;
	if((*list) == NULL) return 0;
	for(i = 0; (*list)[i] != NULL; i++);
	return i;
}

static void ms_add_buffer(ms_buffer_t*** list, ms_buffer_t* buf) {
	ms_buffer_t** old = *list;
	int	      i;
	int	      c = ms_buffer_length(list);
	if(old == NULL) {
		*list	   = malloc(sizeof(*old) * 2);
		(*list)[0] = buf;
		(*list)[1] = NULL;
		return;
	}

	*list = malloc(sizeof(*old) * (c + 2));
	for(i = 0; old[i] != NULL; i++) {
		(*list)[i] = old[i];
	}
	(*list)[i]     = buf;
	(*list)[i + 1] = NULL;

	free(old);
}

static void ms_buffer_delete(ms_buffer_t*** list) {
	ms_buffer_t** old = *list;
	int	      i;
	int	      c = ms_buffer_length(list) - 1;

	*list = malloc(sizeof(*old) * (c + 1));

	for(i = 1; old[i] != NULL; i++) {
		(*list)[i - 1] = old[i];
	}
	(*list)[c] = NULL;

	free(old);
}

MSDEF int ms_socket(const char* type) {
	int sock = -1;
	int v;
	if(strcmp(type, "tcp") == 0) {
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	} else if(strcmp(type, "udp") == 0) {
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}

#ifdef _WIN32
	if(sock == INVALID_SOCKET) sock = -1;
#endif

	if(sock == -1) return -1;

	v = 65535;
	setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&v, sizeof(v));
	v = 65535;
	setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&v, sizeof(v));

	if(strcmp(type, "tcp") == 0) {
		v = 1;
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&v, sizeof(v));
	}

	return sock;
}

MSDEF char* ms_error(int code) {
	char* r;
#ifdef _WIN32
	void* msg;
	int   i;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&msg, 0, 0);
	r = malloc(strlen((char*)msg) + 1);
	strcpy(r, (char*)msg);
	LocalFree(msg);

	for(i = strlen(r) - 1; i >= 0; i--) {
		char old = r[i];
		r[i]	 = 0;
		if(old == '.') break;
	}
#else
	char* str = (char*)strerror(code);
	r	  = malloc(strlen(str) + 1);
	strcpy(r, str);
#endif
	return r;
}

MSDEF int ms_get_error(void) {
#ifdef _WIN32
	return WSAGetLastError();
#else
	return errno;
#endif
}

MSDEF ms_buffer_t* ms_wbuffer(ms_interface_t* net, size_t size) {
	ms_buffer_t* r = malloc(sizeof(*r));
	r->data	       = malloc(size);
	r->size	       = size;
	r->_size       = size;
	r->seek	       = 0;

	ms_add_buffer(&net->wqueue, r);

	return r;
}

MSDEF ms_buffer_t* ms_rbuffer(ms_interface_t* net, size_t size) {
	ms_buffer_t* r = malloc(sizeof(*r));
	r->data	       = malloc(size);
	r->size	       = size;
	r->_size       = size;
	r->seek	       = 0;

	ms_add_buffer(&net->rqueue, r);

	return r;
}

MSDEF void ms_destroy(ms_interface_t* net) {
	if(net->rqueue != NULL) {
		int i;
		for(i = 0; net->rqueue[i] != NULL; i++) {
			free(net->rqueue[i]->data);
			free(net->rqueue[i]);
		}
		free(net->rqueue);
	}
	if(net->wqueue != NULL) {
		int i;
		for(i = 0; net->wqueue[i] != NULL; i++) {
			free(net->wqueue[i]->data);
			free(net->wqueue[i]);
		}
		free(net->wqueue);
	}

	ms_close(net->sock);

	free(net);
}

MSDEF ms_interface_t* ms_tcp(const char* host, int port) {
	ms_interface_t* r = ms_init(host, port);
	if(r != NULL) r->tcp = 1;

	return r;
}

MSDEF int ms_step(ms_interface_t* net) {
	if(net->state >= MS_STATE_FAILED) return 1;

	if(net->state == MS_STATE_PRE_CONNECT) {
		struct sockaddr_in addr;
		int		   st;
		int		   r;
		if(net->sock != -1) {
			ms_close(net->sock);
			net->sock = -1;
		}

		if(net->length <= net->index) {
			net->state = MS_STATE_FAILED_CONNECT;
			return 1;
		}

		net->sock = ms_socket(net->tcp ? "tcp" : "udp");
		if(net->sock == -1) {
			net->state = MS_STATE_FAILED_SOCKET;
			return 1;
		}
		ms_non_block(net->sock);

		memset(&addr, 0, sizeof(addr));
		addr.sin_family	     = AF_INET;
		addr.sin_port	     = htons(net->port);
		addr.sin_addr.s_addr = net->address[net->index++];

		st = connect(net->sock, (struct sockaddr*)&addr, sizeof(addr));
		r  = ms_get_error();
		if(st >= 0 || (_MS_IS_FINE_ERROR(r) || r == _MS_EINTR)) {
			net->state = MS_STATE_CONNECT;
			net->last  = time(NULL);
		}
	} else if(net->state == MS_STATE_CONNECT) {
		/* HACK: unreadable */
		time_t	       t = time(NULL);
		fd_set	       fds;
		struct timeval tv;
		int	       st;
		int	       conn = 0;
		int	       r;
		int	       len = sizeof(r);
		tv.tv_sec	   = 0;
		tv.tv_usec	   = 0;
		FD_ZERO(&fds);
		FD_SET(net->sock, &fds);

		st = getsockopt(net->sock, SOL_SOCKET, SO_ERROR, (void*)&r, &len);
		if(r != 0 && !_MS_IS_FINE_ERROR(r)) {
			net->state = MS_STATE_PRE_CONNECT;
		} else {
			st = select(FD_SETSIZE, NULL, &fds, NULL, &tv);

			if(st > 0) {
				conn = r == 0 ? 1 : 0;
			}
			if(!conn) {
				if((t - net->last) >= MS_TIMEOUT) {
					net->state = MS_STATE_PRE_CONNECT;
				}
			}
		}
		if(conn) {
			net->state = MS_STATE_CONNECTED;
		}
	} else if(net->state == MS_STATE_CONNECTED) {
		if(ms_buffer_length(&net->wqueue) > 0) {
			net->state = MS_STATE_WRITE;
		} else if(ms_buffer_length(&net->rqueue) > 0) {
			net->state = MS_STATE_READ;
		}
	} else if(net->state == MS_STATE_WRITE) {
		ms_buffer_t* buf = net->wqueue[0];
		int	     s	 = send(net->sock, (unsigned char*)buf->data + buf->seek, buf->_size - buf->seek, 0);
		int	     r	 = 0;
		if(s < 0) r = ms_get_error();
		if((s < 0 && !_MS_IS_FINE_ERROR(r)) || s == 0) {
			buf->size  = buf->seek;
			net->state = MS_STATE_WRITE_PART;
		} else if(s > 0) {
			buf->seek += s;
			if(buf->seek >= buf->_size) net->state = MS_STATE_WRITE_COMPLETE;
		}
	} else if(net->state == MS_STATE_WRITE_COMPLETE || net->state == MS_STATE_WRITE_PART) {
		net->state = MS_STATE_AFTER_WRITE;
	} else if(net->state == MS_STATE_AFTER_WRITE) {
		int st = net->wqueue[0]->seek != net->wqueue[0]->_size ? MS_STATE_FAILED_WRITE : MS_STATE_CONNECTED;

		free(net->wqueue[0]->data);
		free(net->wqueue[0]);

		ms_buffer_delete(&net->wqueue);
		net->state = st;
	} else if(net->state == MS_STATE_READ) {
		ms_buffer_t* buf = net->rqueue[0];
		int	     s	 = recv(net->sock, (unsigned char*)buf->data + buf->seek, buf->_size - buf->seek, 0);
		int	     r	 = 0;
		if(s < 0) r = ms_get_error();
		if((s < 0 && !_MS_IS_FINE_ERROR(r)) || s == 0) {
			buf->size  = buf->seek;
			net->state = MS_STATE_READ_PART;
		} else if(s > 0) {
			buf->seek += s;
			if(buf->seek >= buf->_size) net->state = MS_STATE_READ_COMPLETE;
		}
	} else if(net->state == MS_STATE_READ_COMPLETE || net->state == MS_STATE_READ_PART) {
		net->state = MS_STATE_AFTER_READ;
	} else if(net->state == MS_STATE_AFTER_READ) {
		int st = net->rqueue[0]->seek != net->rqueue[0]->_size ? MS_STATE_FAILED_READ : MS_STATE_CONNECTED;

		free(net->rqueue[0]->data);
		free(net->rqueue[0]);

		ms_buffer_delete(&net->rqueue);
		net->state = st;
	}
	return 0;
}

#endif

#ifdef __cplusplus
}
#endif

#endif

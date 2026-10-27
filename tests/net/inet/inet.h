#pragma once

// Shared code for the sceNetInet tests: declarations matching net-imports.S (rather than the SDK's
// headers, whose types don't all match the firmware), startup, and helpers that print a result
// together with sceNetInetGetErrno.

#include <common.h>
#include <pspkernel.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <string.h>

#define INET_AF_INET 2
#define INET_SOCK_STREAM 1
#define INET_SOCK_DGRAM 2
#define INET_SOCK_RAW 3

#define INET_SOL_SOCKET 0xFFFF
#define INET_SO_DEBUG 0x0001
#define INET_SO_ACCEPTCONN 0x0002
#define INET_SO_REUSEADDR 0x0004
#define INET_SO_KEEPALIVE 0x0008
#define INET_SO_DONTROUTE 0x0010
#define INET_SO_BROADCAST 0x0020
#define INET_SO_LINGER 0x0080
#define INET_SO_OOBINLINE 0x0100
#define INET_SO_REUSEPORT 0x0200
#define INET_SO_SNDBUF 0x1001
#define INET_SO_RCVBUF 0x1002
#define INET_SO_SNDLOWAT 0x1003
#define INET_SO_RCVLOWAT 0x1004
#define INET_SO_SNDTIMEO 0x1005
#define INET_SO_RCVTIMEO 0x1006
#define INET_SO_ERROR 0x1007
#define INET_SO_TYPE 0x1008
#define INET_SO_NBIO 0x1009

#define INET_IPPROTO_IP 0
#define INET_IPPROTO_TCP 6
#define INET_IPPROTO_UDP 17
#define INET_IP_TTL 4
#define INET_TCP_NODELAY 1

#define INET_MSG_OOB 0x1
#define INET_MSG_PEEK 0x2
#define INET_MSG_DONTROUTE 0x4
#define INET_MSG_WAITALL 0x40
#define INET_MSG_DONTWAIT 0x80

#define INET_SHUT_RD 0
#define INET_SHUT_WR 1
#define INET_SHUT_RDWR 2

#define LOOPBACK 0x7F000001

typedef struct {
	u8 len;
	u8 family;
	u16 port;
	u32 addr;
	u8 zero[8];
} InetAddr;

typedef struct {
	s32 sec;
	s32 usec;
} InetTimeval;

typedef struct {
	int fd;
	short events;
	short revents;
} InetPollFd;

// The PSP's fd_set has 256 bits.
typedef struct {
	u32 bits[8];
} InetFdSet;

#ifdef __cplusplus
extern "C" {
#endif

int sceNetInit(int poolsize, int calloutprio, int calloutstack, int netintrprio, int netintrstack);
int sceNetTerm(void);
int sceNetInetInit(void);
int sceNetInetTerm(void);
int sceNetInetSocket(int domain, int type, int protocol);
int sceNetInetBind(int s, const InetAddr *addr, int addrlen);
int sceNetInetListen(int s, int backlog);
int sceNetInetAccept(int s, InetAddr *addr, int *addrlen);
int sceNetInetConnect(int s, const InetAddr *addr, int addrlen);
int sceNetInetSend(int s, const void *buf, int len, int flags);
int sceNetInetSendto(int s, const void *buf, int len, int flags, const InetAddr *to, int tolen);
int sceNetInetRecv(int s, void *buf, int len, int flags);
int sceNetInetRecvfrom(int s, void *buf, int len, int flags, InetAddr *from, int *fromlen);
int sceNetInetClose(int s);
int sceNetInetCloseWithRST(int s);
int sceNetInetShutdown(int s, int how);
int sceNetInetGetsockname(int s, InetAddr *addr, int *addrlen);
int sceNetInetGetpeername(int s, InetAddr *addr, int *addrlen);
int sceNetInetSetsockopt(int s, int level, int name, const void *val, int len);
int sceNetInetGetsockopt(int s, int level, int name, void *val, int *len);
int sceNetInetSelect(int nfds, InetFdSet *r, InetFdSet *w, InetFdSet *e, InetTimeval *timeout);
int sceNetInetPoll(InetPollFd *fds, int nfds, int timeoutMs);
int sceNetInetGetErrno(void);
int sceNetInetGetPspError(void);
int sceNetInetSocketAbort(int s);
u32 sceNetInetInetAddr(const char *cp);
int sceNetInetInetAton(const char *cp, u32 *addr);
const char *sceNetInetInetNtop(int af, const void *src, char *dst, int size);
int sceNetInetInetPton(int af, const char *src, void *dst);
int sceNetInetGetTcpcbstat(int *size, void *buf);
int sceNetInetGetUdpcbstat(int *size, void *buf);

#ifdef __cplusplus
}
#endif

static inline u16 inetHtons(u16 v) {
	return (u16)((v >> 8) | (v << 8));
}

static inline u32 inetHtonl(u32 v) {
	return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

static inline InetAddr inetMakeAddr(u32 ip, u16 port) {
	InetAddr a;
	memset(&a, 0, sizeof(a));
	a.len = sizeof(a);
	a.family = INET_AF_INET;
	a.port = inetHtons(port);
	a.addr = inetHtonl(ip);
	return a;
}

// Prints "title: result" and, for a negative result, the errno.
static inline int inetPrint(const char *title, int result) {
	if (result < 0) {
		printf("%s: %d, errno %d\n", title, result, sceNetInetGetErrno());
	} else {
		printf("%s: %d\n", title, result);
	}
	return result;
}

// Waits up to timeoutMs for fd to be readable (or writable). Returns select()'s result.
static inline int inetWait(int fd, int writable, int timeoutMs) {
	InetFdSet set;
	memset(&set, 0, sizeof(set));
	set.bits[fd >> 5] |= 1U << (fd & 31);
	InetTimeval tv = { timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
	return sceNetInetSelect(fd + 1, writable ? NULL : &set, writable ? &set : NULL, NULL, &tv);
}

static inline int inetStartup() {
	setvbuf(stdout, NULL, _IONBF, 0);
	int result = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	if (result >= 0) {
		result = sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	}
	if (result >= 0) {
		result = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
	}
	if (result >= 0) {
		result = sceNetInetInit();
	}
	if (result < 0) {
		printf("Startup failed: %08x\n", result);
		return 0;
	}
	return 1;
}

static inline void inetShutdown() {
	sceNetInetTerm();
	sceNetTerm();
	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}

// A connected pair of TCP sockets over loopback, through a listener on port. Returns false if that failed.
static inline int inetTcpPair(u16 port, int *listener, int *client, int *server) {
	*listener = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	InetAddr addr = inetMakeAddr(LOOPBACK, port);
	if (sceNetInetBind(*listener, &addr, sizeof(addr)) < 0 || sceNetInetListen(*listener, 4) < 0) {
		return 0;
	}
	*client = sceNetInetSocket(INET_AF_INET, INET_SOCK_STREAM, 0);
	if (sceNetInetConnect(*client, &addr, sizeof(addr)) < 0) {
		return 0;
	}
	*server = sceNetInetAccept(*listener, NULL, NULL);
	return *server >= 0;
}

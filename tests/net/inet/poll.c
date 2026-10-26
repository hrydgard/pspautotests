// sceNetInetPoll with an fd that isn't open, next to one that is, and sceNetInetGetsockname with a
// namelen too small to hold even the family.

#include <common.h>
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_inet.h>
#include <psputility.h>
#include <psputility_netmodules.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 23470
#define POLLIN 0x001
#define POLLOUT 0x004

static void testPoll(const char *title, struct SceNetInetPollfd *fds, int n) {
	for (int i = 0; i < n; ++i)
		fds[i].revents = 0x7777;
	int result = sceNetInetPoll(fds, n, 0);
	printf("%s: %d, errno %d", title, result, result < 0 ? sceNetInetGetErrno() : 0);
	for (int i = 0; i < n; ++i)
		printf(", revents %04x", (u16)fds[i].revents);
	printf("\n");
}

static void testGetsockname(const char *title, int sock, int namelen) {
	u8 buf[32];
	memset(buf, 0xCC, sizeof(buf));
	socklen_t len = namelen;
	int result = sceNetInetGetsockname(sock, (struct sockaddr *)buf, &len);
	// sa_len, sa_family, then the port.
	printf("%s: %d, errno %d, len %d, bytes %02x %02x %02x %02x, byte 16 %02x\n", title, result, result < 0 ? sceNetInetGetErrno() : 0,
		(int)len, buf[0], buf[1], buf[2], buf[3], buf[16]);
}

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("load common: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON));
	printf("load inet: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_INET));
	int result = sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000);
	if (result < 0) {
		printf("sceNetInit: %08x\n", result);
		return 1;
	}
	result = sceNetInetInit();
	if (result < 0) {
		printf("sceNetInetInit: %08x\n", result);
		sceNetTerm();
		return 1;
	}

	int sock = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = htonl(0x7F000001);
	printf("bind: %d\n", sceNetInetBind(sock, (struct sockaddr *)&addr, sizeof(addr)));

	struct SceNetInetPollfd fds[2];
	fds[0].fd = sock;
	fds[0].events = POLLOUT;
	testPoll("Open socket, POLLOUT", fds, 1);

	fds[0].fd = 200;
	fds[0].events = POLLIN;
	testPoll("Unopened fd 200, POLLIN", fds, 1);

	fds[0].fd = sock;
	fds[0].events = POLLOUT;
	fds[1].fd = 200;
	fds[1].events = POLLIN;
	testPoll("Open socket and unopened fd", fds, 2);

	testGetsockname("getsockname, namelen 16", sock, 16);
	testGetsockname("getsockname, namelen 3", sock, 3);
	testGetsockname("getsockname, namelen 2", sock, 2);
	testGetsockname("getsockname, namelen 1", sock, 1);
	testGetsockname("getsockname, namelen 0", sock, 0);

	sceNetInetClose(sock);
	sceNetInetTerm();
	sceNetTerm();
	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
	return 0;
}

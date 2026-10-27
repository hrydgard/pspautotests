// The address conversion functions: sceNetInetInetAddr, InetAton, InetPton and InetNtop, on
// ordinary and edge-case input.

#include "inet.h"

static const char *inputs[] = {
	"127.0.0.1",
	"0.0.0.0",
	"255.255.255.255",
	"192.168.1.20",
	"1.2.3",
	"1.2",
	"16909060",
	"0x7f.1",
	"0x7f000001",
	"010.0.0.1",
	"256.0.0.1",
	"1.2.3.4.5",
	"1.2.3.4 ",
	" 1.2.3.4",
	"1.2.3.",
	"",
	"abc",
	"1..2.3",
};

int main(int argc, char *argv[]) {
	if (!inetStartup()) {
		return 1;
	}

	printf("InetAddr / InetAton / InetPton:\n");
	for (int i = 0; i < (int)ARRAY_SIZE(inputs); ++i) {
		u32 addr = sceNetInetInetAddr(inputs[i]);
		u32 aton = 0xCCCCCCCC;
		int atonResult = sceNetInetInetAton(inputs[i], &aton);
		u32 pton = 0xCCCCCCCC;
		int ptonResult = sceNetInetInetPton(INET_AF_INET, inputs[i], &pton);
		printf("  '%s': addr %08lx, aton %d %08lx, pton %d %08lx\n", inputs[i], addr, atonResult, aton, ptonResult, pton);
	}

	printf("\nInetPton, other families:\n");
	u32 pton = 0xCCCCCCCC;
	int result = sceNetInetInetPton(0, "1.2.3.4", &pton);
	printf("  Family 0: %d, errno %d, %08lx\n", result, result < 0 ? sceNetInetGetErrno() : 0, pton);
	u8 v6[16];
	memset(v6, 0xCC, sizeof(v6));
	result = sceNetInetInetPton(23, "::1", v6);
	printf("  Family 23 '::1': %d, errno %d, last byte %02x\n", result, result < 0 ? sceNetInetGetErrno() : 0, v6[15]);

	printf("\nInetNtop:\n");
	static const u32 addrs[] = { 0x0100007F, 0x00000000, 0xFFFFFFFF, 0x1401A8C0 };
	for (int i = 0; i < (int)ARRAY_SIZE(addrs); ++i) {
		char buf[32];
		memset(buf, 'X', sizeof(buf));
		buf[31] = 0;
		const char *r = sceNetInetInetNtop(INET_AF_INET, &addrs[i], buf, sizeof(buf));
		printf("  %08lx: %s '%s'\n", addrs[i], r == buf ? "returns dst" : (r == NULL ? "NULL" : "other"), r ? buf : "");
	}
	char small[8];
	memset(small, 'X', sizeof(small));
	const char *r = sceNetInetInetNtop(INET_AF_INET, &addrs[3], small, sizeof(small));
	printf("  Buffer of 8 for 192.168.1.20: %s, errno %d, first bytes %.4s\n", r == NULL ? "NULL" : "not NULL", r == NULL ? sceNetInetGetErrno() : 0, small);
	char exact[16];
	r = sceNetInetInetNtop(INET_AF_INET, &addrs[2], exact, 16);
	printf("  Buffer of 16 for 255.255.255.255: %s\n", r == NULL ? "NULL" : exact);
	r = sceNetInetInetNtop(INET_AF_INET, &addrs[2], exact, 15);
	printf("  Buffer of 15 for 255.255.255.255: %s\n", r == NULL ? "NULL" : exact);
	r = sceNetInetInetNtop(0, &addrs[0], exact, 16);
	printf("  Family 0: %s, errno %d\n", r == NULL ? "NULL" : "not NULL", r == NULL ? sceNetInetGetErrno() : 0);

	inetShutdown();
	return 0;
}

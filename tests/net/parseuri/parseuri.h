#include <common.h>
#include <psputility.h>
#include <psputility_netmodules.h>

// The struct sceUriParse fills in. Everything but noSlash and port is a pointer into the work
// area the caller supplies, so the tests print offsets into that area rather than addresses.
typedef struct {
	int noSlash;
	unsigned int scheme;
	unsigned int userName;
	unsigned int password;
	unsigned int host;
	unsigned int path;
	unsigned int query;
	unsigned int fragment;
	unsigned short port;
	unsigned char unknown[10];
} ParsedUri;

extern "C" {
int sceUriParse(ParsedUri *parsed, const char *uri, void *workArea, int *workAreaSize, int workAreaLength);
int sceUriBuild(void *out, int *outSize, int outLength, const ParsedUri *parsed, int flags);
int sceUriEscape(void *escaped, int *escapedLength, int escapedBufferLength, const char *source);
int sceUriUnescape(void *unescaped, int *unescapedLength, int unescapedBufferLength, const char *source);
}

// libparse_uri.prx imports nothing but Kernel_Library, so it needs none of the rest of the net
// stack - loading PSP_NET_MODULE_COMMON as well brings up wlan for no reason.
static inline void loadParseUri() {
	// Unbuffered, so that if a case wedges the machine the lines before it have already reached
	// the host and say which one it was.
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("sceUtilityLoadNetModule(parseuri): %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_PARSEURI));
}

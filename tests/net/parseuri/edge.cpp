// sceUriParse on the awkward inputs: missing pieces, null arguments, a work area too small, and
// the ports that don't fit.
//
// Kept apart from parse.cpp so that if one of these wedges the machine it doesn't take the
// ordinary cases with it, and printed line by line for the same reason.

#include "parseuri.h"

static char workArea[1024] __attribute__((aligned(64)));
static ParsedUri parsed __attribute__((aligned(64)));

// Short form: just the pieces, so a screenful covers a lot of cases.
static void brief(const char *uri) {
	memset(workArea, 0, sizeof(workArea));
	memset(&parsed, 0, sizeof(parsed));
	int workAreaSize = -1;

	printf("  \"%s\"", uri);
	int result = sceUriParse(&parsed, uri, workArea, &workAreaSize, sizeof(workArea));
	if (result != 0) {
		printf(" -> %08x\n", result);
		return;
	}
	printf(" -> noSlash=%d port=%d size=%d\n", parsed.noSlash, parsed.port, workAreaSize);
	printf("     scheme=\"%s\" user=\"%s\" pass=\"%s\" host=\"%s\"\n",
		(const char *)(size_t)parsed.scheme, (const char *)(size_t)parsed.userName,
		(const char *)(size_t)parsed.password, (const char *)(size_t)parsed.host);
	printf("     path=\"%s\" query=\"%s\" fragment=\"%s\"\n",
		(const char *)(size_t)parsed.path, (const char *)(size_t)parsed.query,
		(const char *)(size_t)parsed.fragment);
}

extern "C" int main(int argc, char *argv[]) {
	loadParseUri();

	printf("Missing pieces\n");
	brief("");
	brief("http://");
	brief("http:");
	brief("//example.com/path");
	brief("/just/a/path");
	brief("relative/path");
	brief("example.com");

	printf("Ports\n");
	brief("http://example.com:0/");
	brief("http://example.com:1/");
	brief("http://example.com:65535/");
	brief("http://example.com:65536/");
	brief("http://example.com:99999/");
	brief("http://example.com:abc/");
	brief("http://example.com:/");

	// The theory for why urn:isbn:0451450523 is rejected: with no "//" the part after the scheme
	// is still read as host:port, and that number does not fit a port.
	printf("Colons after a scheme with no slashes\n");
	brief("urn:isbn:1");
	brief("urn:isbn:65535");
	brief("urn:isbn:65536");
	brief("urn:isbn:0451450523");

	// 65536 is accepted and wraps to 0 while 99999 is refused, so the limit is somewhere
	// between; these say where.
	printf("Port limit\n");
	brief("http://example.com:65537/");
	brief("http://example.com:65600/");
	brief("http://example.com:70000/");
	brief("http://example.com:99998/");
	brief("http://example.com:100000/");
	brief("http://example.com:000080/");

	// A bracketed address is not read as an authority at all, so something about the first
	// character decides whether there is one. These say what.
	printf("What can start a host\n");
	brief("http://[::1]:8080/path");
	brief("http://1.2.3.4/path");
	brief("http://-abc/path");
	brief("http://_abc/path");
	brief("http://%41bc/path");
	brief("http://a[b/path");
	brief("http://a b/path");

	printf("Null arguments\n");
	{
		int workAreaSize = -1;
		printf("  no work area: %08x", sceUriParse(&parsed, "http://example.com/a?b#c", NULL, &workAreaSize, 0));
		printf(" size=%d\n", workAreaSize);
		workAreaSize = -1;
		printf("  no parsed area: %08x", sceUriParse(NULL, "http://example.com/a?b#c", workArea, &workAreaSize, sizeof(workArea)));
		printf(" size=%d\n", workAreaSize);
		printf("  neither, no size out: %08x\n", sceUriParse(NULL, "http://example.com/", NULL, NULL, 0));
		printf("  null uri: %08x\n", sceUriParse(&parsed, NULL, workArea, &workAreaSize, sizeof(workArea)));
	}

	// Last, because a length the library does not respect would land on whatever follows.
	printf("Work area too small\n");
	{
		int workAreaSize = -1;
		memset(workArea, 0, sizeof(workArea));
		int result = sceUriParse(&parsed, "http://example.com/path", workArea, &workAreaSize, 4);
		printf("  4 bytes: %08x size=%d\n", result, workAreaSize);
		workAreaSize = -1;
		memset(workArea, 0, sizeof(workArea));
		result = sceUriParse(&parsed, "http://example.com/path", workArea, &workAreaSize, 0);
		printf("  0 bytes: %08x size=%d\n", result, workAreaSize);
	}

	printf("Done\n");
	return 0;
}

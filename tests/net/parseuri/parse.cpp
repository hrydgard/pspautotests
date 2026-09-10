// What sceUriParse actually splits a URI into, and where it puts the pieces.
//
// Every field but noSlash and port is a pointer into the caller's work area, so what is printed
// is the offset into that area and the string found there - addresses themselves are not the
// same twice.
//
// This uses printf rather than schedf on purpose: the output goes to the host as it is produced,
// so if a case wedges the PSP the lines before it survive and say which one it was.

#include "parseuri.h"

static char workArea[1024] __attribute__((aligned(64)));
static ParsedUri parsed __attribute__((aligned(64)));

static void dumpField(const char *name, unsigned int addr) {
	if (addr == 0) {
		printf("    %-8s null\n", name);
		return;
	}
	const unsigned int base = (unsigned int)(size_t)workArea;
	if (addr >= base && addr < base + sizeof(workArea)) {
		printf("    %-8s +%03d \"%s\"\n", name, (int)(addr - base), (const char *)(size_t)addr);
	} else {
		// Somewhere other than the work area, which would be a surprise worth seeing.
		printf("    %-8s elsewhere \"%s\"\n", name, (const char *)(size_t)addr);
	}
}

static void testParse(const char *uri) {
	memset(workArea, 0, sizeof(workArea));
	memset(&parsed, 0, sizeof(parsed));
	int workAreaSize = -1;

	printf("  \"%s\"\n", uri);
	int result = sceUriParse(&parsed, uri, workArea, &workAreaSize, sizeof(workArea));
	printf("    result=%08x size=%d\n", result, workAreaSize);
	if (result != 0) {
		return;
	}

	printf("    noSlash=%d port=%d\n", parsed.noSlash, parsed.port);
	dumpField("scheme", parsed.scheme);
	dumpField("user", parsed.userName);
	dumpField("password", parsed.password);
	dumpField("host", parsed.host);
	dumpField("path", parsed.path);
	dumpField("query", parsed.query);
	dumpField("fragment", parsed.fragment);

	// Ten bytes past the port that nothing has identified. Printed so a non-zero one is noticed.
	printf("    trailing:");
	for (unsigned int i = 0; i < ARRAY_SIZE(parsed.unknown); ++i) {
		printf(" %02x", parsed.unknown[i]);
	}
	printf("\n");
}

extern "C" int main(int argc, char *argv[]) {
	loadParseUri();

	printf("Everything present\n");
	testParse("http://user:pass@example.com:8080/path/to/file.html?a=1&b=2#frag");

	printf("The ordinary shapes\n");
	testParse("http://example.com/");
	testParse("http://example.com");
	testParse("http://example.com/path");
	testParse("http://example.com:80/path");
	testParse("https://example.com/a/b/c?q=1");
	testParse("http://example.com/#only-fragment");
	testParse("http://example.com/?only-query");
	testParse("http://user@example.com/");
	testParse("HTTP://EXAMPLE.COM/PATH");

	printf("No authority\n");
	testParse("mailto:someone@example.com");
	testParse("urn:isbn:0451450523");
	testParse("file:///tmp/x");
	testParse("about:blank");

	printf("Done\n");
	return 0;
}

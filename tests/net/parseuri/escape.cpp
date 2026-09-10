// sceUriEscape and sceUriUnescape: which characters get percent-encoded, what comes back out,
// and what the length argument means.
//
// Strings are printed with anything outside printable ASCII rendered as \xNN, so the recorded
// output stays plain text whatever the library produces.

#include "parseuri.h"

static char out[512] __attribute__((aligned(64)));

static const char *printable(const char *s, char *buf, int bufSize) {
	int o = 0;
	for (; *s != '\0' && o < bufSize - 5; ++s) {
		unsigned char c = (unsigned char)*s;
		if (c >= 0x20 && c < 0x7f && c != '\\') {
			buf[o++] = (char)c;
		} else {
			o += sprintf(buf + o, "\\x%02X", c);
		}
	}
	buf[o] = '\0';
	return buf;
}

static void escape(const char *source) {
	char a[512], b[512];
	memset(out, 0, sizeof(out));
	int length = -1;
	int result = sceUriEscape(out, &length, sizeof(out), source);
	printf("  \"%s\" -> %08x len=%d \"%s\"\n", printable(source, a, sizeof(a)), result, length,
		printable(out, b, sizeof(b)));
}

static void unescape(const char *source) {
	char a[512], b[512];
	memset(out, 0, sizeof(out));
	int length = -1;
	int result = sceUriUnescape(out, &length, sizeof(out), source);
	printf("  \"%s\" -> %08x len=%d \"%s\"\n", printable(source, a, sizeof(a)), result, length,
		printable(out, b, sizeof(b)));
}

// A round trip should come back to where it started.
static void roundTrip(const char *source) {
	static char mid[512];
	char a[512], b[512];
	int length = -1;
	memset(mid, 0, sizeof(mid));
	if (sceUriEscape(mid, &length, sizeof(mid), source) != 0) {
		printf("  \"%s\" -> escape failed\n", printable(source, a, sizeof(a)));
		return;
	}
	memset(out, 0, sizeof(out));
	length = -1;
	int result = sceUriUnescape(out, &length, sizeof(out), mid);
	printf("  \"%s\" -> \"%s\" -> %08x %s\n", printable(source, a, sizeof(a)),
		printable(mid, b, sizeof(b)), result, strcmp(source, out) == 0 ? "same" : "DIFFERENT");
}

extern "C" int main(int argc, char *argv[]) {
	loadParseUri();

	printf("Escape\n");
	escape("");
	escape("abc");
	escape("ABCabc012");
	escape("a b");
	escape("a+b");
	escape("hello world!");
	escape("a/b?c#d&e=f");
	escape("-_.~");
	escape("!*'();:@&=+$,/?#[]");
	escape("%");
	escape("%41");
	escape("\x01\x1f\x7f");
	escape("\x80\xff");
	escape("caf\xc3\xa9");

	printf("Unescape\n");
	unescape("");
	unescape("abc");
	unescape("a%20b");
	unescape("a+b");
	unescape("%41%42%43");
	unescape("%2f%2F");
	unescape("%");
	unescape("%4");
	unescape("%zz");
	unescape("%00abc");
	unescape("100%25");

	printf("Round trip\n");
	roundTrip("hello world!");
	roundTrip("a/b?c#d&e=f");
	roundTrip("caf\xc3\xa9");

	// A null output buffer is the size query and is guarded for. A null length pointer or a null
	// source are not - the library writes through the one and reads through the other with no
	// check at all - so they are left alone deliberately.
	printf("Sizes\n");
	{
		int length = -1;
		printf("  escape, no buffer: %08x", sceUriEscape(NULL, &length, 0, "hello world!"));
		printf(" len=%d\n", length);
		length = -1;
		printf("  unescape, no buffer: %08x", sceUriUnescape(NULL, &length, 0, "a%20b"));
		printf(" len=%d\n", length);
	}

	// Last, in case a length the library does not respect lands past the buffer.
	printf("Buffer too small\n");
	{
		char b[512];
		int length = -1;
		memset(out, 0, sizeof(out));
		int result = sceUriEscape(out, &length, 4, "hello world!");
		printf("  escape into 4: %08x len=%d \"%s\"\n", result, length, printable(out, b, sizeof(b)));
		length = -1;
		memset(out, 0, sizeof(out));
		result = sceUriUnescape(out, &length, 4, "hello%20world");
		printf("  unescape into 4: %08x len=%d \"%s\"\n", result, length, printable(out, b, sizeof(b)));
	}

	printf("Done\n");
	return 0;
}

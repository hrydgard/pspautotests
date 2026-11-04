// What sceNpAuthGetTicketParam reads out of a ticket, given hand-built tickets (a real one needs
// PSN). Tickets are big-endian: an 8-byte header (version, size), a 4-byte section delimiter
// (type, size), then params of type, length and value.
//
// Each call gets a 260-byte buffer filled with 0xAA, and what it left there is summarised: the
// first bytes, then how many of the 256 are still 0xAA, and whether the 4 past them were touched.

#include <common.h>
#include <psputility.h>
#include <psputility_modules.h>
#include <psputility_netmodules.h>

extern "C" {
int sceNpInit();
int sceNpTerm();
int sceNpAuthInit(int poolSize, int stackSize, int threadPrio);
int sceNpAuthTerm();
int sceNpAuthGetTicketParam(const void *ticket, int ticketLen, int paramNum, void *buffer);
}

static unsigned char ticket[1024] __attribute__((aligned(16)));
static int ticketLen;
static unsigned char out[260] __attribute__((aligned(16)));

static void put16(int v) {
	ticket[ticketLen++] = (v >> 8) & 0xFF;
	ticket[ticketLen++] = v & 0xFF;
}

static void put32(unsigned int v) {
	put16(v >> 16);
	put16(v & 0xFFFF);
}

static void putParam(int type, const void *data, int len) {
	put16(type);
	put16(len);
	memcpy(ticket + ticketLen, data, len);
	ticketLen += len;
}

static void putParamU32(int type, unsigned int v) {
	put16(type);
	put16(4);
	put32(v);
}

static void putParamU64(int type, unsigned int hi, unsigned int lo) {
	put16(type);
	put16(8);
	put32(hi);
	put32(lo);
}

static void startTicket(unsigned int version, int sectionType) {
	memset(ticket, 0, sizeof(ticket));
	ticketLen = 0;
	put32(version);
	put32(0);
	put16(sectionType);
	put16(0);
}

// Fills in the two sizes, then pads with zeros like PPSSPP's dummy ticket's footer area.
static void endTicket() {
	int bodySize = ticketLen - 12;
	ticket[10] = (bodySize >> 8) & 0xFF;
	ticket[11] = bodySize & 0xFF;
	ticketLen += 36;
	int size = ticketLen - 8;
	ticket[4] = (size >> 24) & 0xFF;
	ticket[5] = (size >> 16) & 0xFF;
	ticket[6] = (size >> 8) & 0xFF;
	ticket[7] = size & 0xFF;
}

// Like PPSSPP's dummy login ticket.
static void buildStandardTicket(unsigned int version, int sectionType) {
	static const char serial[20] = "ABCDEFGHIJKLMNOPQRS";
	char onlineId[32] = "pspautotests";
	char service[24] = "UP0000-NPXX00000_00";
	startTicket(version, sectionType);
	putParam(8, serial, 20);
	putParamU32(1, 0x00000100);
	putParamU64(7, 0x0000011D, 0x12345678);
	putParamU64(7, 0x0000011D, 0x9ABCDEF0);
	putParamU64(2, 0x592E71C5, 0x46E86859);
	putParam(4, onlineId, 32);
	putParam(8, "us\0\x02", 4);
	putParam(4, "c9\0\0", 4);
	putParam(8, service, 24);
	putParamU32(1, 0x14000000);
	putParam(0, NULL, 0);
	putParam(0, NULL, 0);
	endTicket();
}

struct Param {
	int type;
	int len;
	const void *data;
};

static const unsigned char date1[8] = { 0x00, 0x00, 0x01, 0x1D, 0x12, 0x34, 0x56, 0x78 };
static const unsigned char date2[8] = { 0x00, 0x00, 0x01, 0x1D, 0x9A, 0xBC, 0xDE, 0xF0 };
static const unsigned char long1[8] = { 0x59, 0x2E, 0x71, 0xC5, 0x46, 0xE8, 0x68, 0x59 };
static const char onlineId[32] = "pspautotests";
static const char service[24] = "UP0000-NPXX00000_00";
static char pattern[300];

// The same params as buildStandardTicket, so one slot can be swapped out.
static const Param standardParams[] = {
	{ 8, 20, "ABCDEFGHIJKLMNOPQRS" },
	{ 1, 4, "\x00\x00\x01\x00" },
	{ 7, 8, date1 },
	{ 7, 8, date2 },
	{ 2, 8, long1 },
	{ 4, 32, onlineId },
	{ 8, 4, "us\0\x02" },
	{ 4, 4, "c9\0\0" },
	{ 8, 24, service },
	{ 1, 4, "\x14\x00\x00\x00" },
	{ 0, 0, NULL },
	{ 0, 0, NULL },
};

static void buildTicketWithSlot(int slot, int type, int len, const void *data) {
	startTicket(0x21010000, 0x3000);
	for (int i = 0; i < (int)ARRAY_SIZE(standardParams); ++i) {
		if (i == slot) {
			putParam(type, data, len);
		} else {
			putParam(standardParams[i].type, standardParams[i].data, standardParams[i].len);
		}
	}
	endTicket();
}

static void testParam(const char *title, int len, int paramNum) {
	memset(out, 0xAA, sizeof(out));
	int result = sceNpAuthGetTicketParam(ticket, len, paramNum, out);

	int untouched = 0;
	int last = -1;
	for (int i = 0; i < 256; ++i) {
		if (out[i] == 0xAA) {
			++untouched;
		}
		if (out[i] != 0) {
			last = i;
		}
	}
	bool past = out[256] != 0xAA || out[257] != 0xAA || out[258] != 0xAA || out[259] != 0xAA;

	printf("%s, param %d: %08x", title, paramNum, result);
	int shown = last + 1 < 12 ? last + 1 : 12;
	printf(" [");
	for (int i = 0; i < shown; ++i) {
		printf("%s%02x", i == 0 ? "" : " ", out[i]);
	}
	printf("] last non-zero %d, 0xAA left %d%s\n", last, untouched, past ? ", WROTE PAST 256" : "");
}

static void testStandard(const char *title) {
	for (int i = -1; i <= 12; ++i) {
		testParam(title, ticketLen, i);
	}
}

extern "C" int main(int argc, char *argv[]) {
	printf("Load common: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON));
	printf("Load inet: %08x\n", sceUtilityLoadNetModule(PSP_NET_MODULE_INET));
	printf("Load np: %08x\n", sceUtilityLoadModule(PSP_MODULE_NP_COMMON));

	buildStandardTicket(0x21010000, 0x3000);
	printf("Standard ticket is %d bytes\n", ticketLen);

	testParam("Before init", ticketLen, 1);

	printf("sceNpInit: %08x\n", sceNpInit());
	printf("sceNpAuthInit: %08x\n", sceNpAuthInit(0x20000, 0x4000, 0x28));

	printf("\nStandard ticket:\n");
	testStandard("  2.1");

	printf("\nLengths:\n");
	const int lengths[] = { 0, 4, 8, 11, 12, 15, 16, 36, 37, 39, 40, 48, ticketLen - 36 };
	for (size_t i = 0; i < ARRAY_SIZE(lengths); ++i) {
		char title[32];
		snprintf(title, sizeof(title), "  len %d", lengths[i]);
		testParam(title, lengths[i], 0);
		testParam(title, lengths[i], 1);
		testParam(title, lengths[i], 11);
	}
	testParam("  len -1", -1, 0);

	printf("\nHeader:\n");
	const unsigned int versions[] = { 0x21000000, 0x31000000, 0x41000000, 0x00000000, 0x12345678 };
	for (size_t i = 0; i < ARRAY_SIZE(versions); ++i) {
		char title[32];
		snprintf(title, sizeof(title), "  version %08x", versions[i]);
		buildStandardTicket(versions[i], 0x3000);
		testParam(title, ticketLen, 1);
	}
	const int sections[] = { 0x3002, 0x3010, 0x0000 };
	for (size_t i = 0; i < ARRAY_SIZE(sections); ++i) {
		char title[32];
		snprintf(title, sizeof(title), "  section %04x", sections[i]);
		buildStandardTicket(0x21010000, sections[i]);
		testParam(title, ticketLen, 1);
	}
	buildStandardTicket(0x21010000, 0x3000);
	ticket[10] = 0;
	ticket[11] = 8;
	testParam("  section size 8", ticketLen, 1);
	testParam("  section size 8", ticketLen, 5);
	buildStandardTicket(0x21010000, 0x3000);
	ticket[4] = ticket[5] = ticket[6] = 0;
	ticket[7] = 8;
	testParam("  ticket size 8", ticketLen, 1);
	testParam("  ticket size 8", ticketLen, 5);

	printf("\nOdd params:\n");
	static char longString[300];
	for (size_t i = 0; i < sizeof(longString); ++i) {
		longString[i] = 'a' + (i % 26);
	}
	startTicket(0x21010000, 0x3000);
	putParam(3, "\x11\x22\x33\x44", 4);
	putParam(1, "\x11\x22", 2);
	putParam(2, "\x11\x22\x33\x44", 4);
	putParam(4, longString, 300);
	putParam(8, longString, 255);
	putParam(8, longString, 256);
	putParam(4, "", 0);
	putParam(1, "\x11\x22\x33\x44\x55\x66\x77\x88", 8);
	putParam(0, "\x11\x22\x33\x44", 4);
	putParam(0x3001, "\x11\x22\x33\x44\x55\x66\x77\x88", 8);
	putParam(7, "\x11\x22\x33\x44\x55\x66\x77\x88\x99\xAA", 10);
	putParam(4, "abc", 3);
	endTicket();
	testStandard("  odd");

	printf("\nTypes per slot:\n");
	for (size_t i = 0; i < sizeof(pattern); ++i) {
		pattern[i] = 'a' + (i % 26);
	}
	const int types[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 0x3010, 0x3011 };
	for (int slot = 0; slot <= 10; ++slot) {
		for (size_t i = 0; i < ARRAY_SIZE(types); ++i) {
			char title[48];
			int len = types[i] == 0 ? 0 : (types[i] == 1 ? 4 : 8);
			snprintf(title, sizeof(title), "  slot %d type %x len %d", slot, types[i], len);
			buildTicketWithSlot(slot, types[i], len, pattern);
			testParam(title, ticketLen, slot);
		}
	}

	printf("\nLengths per slot:\n");
	const int slotLengths[] = { 0, 2, 4, 7, 8, 9, 16, 32, 255, 256, 300 };
	for (int slot = 0; slot <= 10; ++slot) {
		for (size_t i = 0; i < ARRAY_SIZE(slotLengths); ++i) {
			char title[48];
			int type = standardParams[slot].type;
			snprintf(title, sizeof(title), "  slot %d type %x len %d", slot, type, slotLengths[i]);
			buildTicketWithSlot(slot, type, slotLengths[i], pattern);
			testParam(title, ticketLen, slot);
		}
	}

	printf("\nA bad slot after the one asked for:\n");
	buildTicketWithSlot(5, 3, 8, pattern);
	testParam("  slot 5 type 3", ticketLen, 2);
	testParam("  slot 5 type 3", ticketLen, 4);
	testParam("  slot 5 type 3", ticketLen, 5);
	testParam("  slot 5 type 3", ticketLen, 6);
	testParam("  len 0 param 12", 0, 12);
	testParam("  len 0 param -1", 0, -1);

	printf("\nType decoding:\n");
	static const unsigned char rising[16] = { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F };
	const int oddTypes[] = { 0x3011, 0x3001, 0x3007, 0x3008, 0x0011, 0x0101, 0x1001, 0x8001, 0x0009, 0x000A, 0x000B, 0x000C, 0x0010, 0x3000, 0x3002 };
	const int oddLengths[] = { 0, 2, 4, 8, 12 };
	for (size_t i = 0; i < ARRAY_SIZE(oddTypes); ++i) {
		for (size_t j = 0; j < ARRAY_SIZE(oddLengths); ++j) {
			char title[48];
			snprintf(title, sizeof(title), "  type %x len %d", oddTypes[i], oddLengths[j]);
			buildTicketWithSlot(0, oddTypes[i], oddLengths[j], rising);
			testParam(title, ticketLen, 0);
		}
	}

	printf("\nCheck order:\n");
	const unsigned int moreVersions[] = { 0x21010001, 0x21020000, 0x31010000, 0x11010000, 0x20010000, 0x22010000 };
	for (size_t i = 0; i < ARRAY_SIZE(moreVersions); ++i) {
		char title[32];
		snprintf(title, sizeof(title), "  version %08x", moreVersions[i]);
		buildStandardTicket(moreVersions[i], 0x3000);
		testParam(title, ticketLen, 1);
	}
	buildStandardTicket(0x12345678, 0x3000);
	testParam("  bad version", ticketLen, 11);
	testParam("  bad version", ticketLen, 12);
	testParam("  bad version len 4", 4, 0);
	testParam("  bad version len 8", 8, 0);
	testParam("  bad version len 12", 12, 0);
	buildStandardTicket(0x21010000, 0x3002);
	testParam("  bad section", ticketLen, 11);
	testParam("  bad section len 8", 8, 0);
	buildStandardTicket(0x21010000, 0x3000);
	testParam("  len 1", 1, 0);
	testParam("  len 3", 3, 0);
	testParam("  len 1 param 11", 1, 11);
	testParam("  len 1 param 12", 1, 12);
	testParam("  len 0x7fffffff param 12", 0x7fffffff, 12);
	const int bigLengths[] = { 0x400, 0x10000, 0x10001, 0x100000 };
	for (size_t i = 0; i < ARRAY_SIZE(bigLengths); ++i) {
		char title[32];
		snprintf(title, sizeof(title), "  len %x", bigLengths[i]);
		testParam(title, bigLengths[i], 1);
	}

	printf("\nsceNpAuthTerm: %08x\n", sceNpAuthTerm());
	printf("sceNpTerm: %08x\n", sceNpTerm());
	sceUtilityUnloadModule(PSP_MODULE_NP_COMMON);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
	return 0;
}

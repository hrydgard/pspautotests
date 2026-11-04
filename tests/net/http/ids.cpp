// How sceHttp hands out template, connection and request ids, what happens to them on delete, and
// which calls accept which kind of id.
//
// Nothing here needs a network: creating connections and requests doesn't resolve or connect.
// printf rather than checkpoint, so the lines before a hang reach the host.

#include <common.h>
#include <pspnet.h>
#include <psphttp.h>
#include <psputility_netmodules.h>

static void loadModules() {
	sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON);
	sceUtilityLoadNetModule(PSP_NET_MODULE_INET);
	sceUtilityLoadNetModule(PSP_NET_MODULE_PARSEHTTP);
	sceUtilityLoadNetModule(PSP_NET_MODULE_PARSEURI);
	sceUtilityLoadNetModule(PSP_NET_MODULE_HTTP);
}

static void unloadModules() {
	sceUtilityUnloadNetModule(PSP_NET_MODULE_HTTP);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_PARSEURI);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_PARSEHTTP);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
	sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}

static int createTemplate() {
	return sceHttpCreateTemplate((char *)"pspautotests", 1, 0);
}

static int createConnection(int tmpl) {
	return sceHttpCreateConnection(tmpl, (char *)"localhost", (char *)"http", 80, 0);
}

static int createRequest(int conn) {
	return sceHttpCreateRequest(conn, PSP_HTTP_METHOD_GET, (char *)"/", 0);
}

// Deleting a header that isn't there works on every kind of id, so it tells live ids from dead ones.
static const char *probe(int id) {
	int result = sceHttpDeleteHeader(id, "X-None");
	if (result == (int)0x80431025) {
		return "live";
	}
	if (result == (int)0x80431100) {
		return "invalid";
	}
	static char buf[16];
	snprintf(buf, sizeof(buf), "%08x", result);
	return buf;
}

// Calls that take an id, each with harmless arguments.
static void testCalls(const char *title, int id) {
	int status = -1;
	SceULong64 length = 0;
	unsigned char *header = NULL;
	unsigned int headerSize = 0;
	char data[16];

	printf("%s (%08x):\n", title, id);
	printf("  SetResolveTimeOut: %08x\n", sceHttpSetResolveTimeOut(id, 1000000));
	printf("  SetResolveRetry: %08x\n", sceHttpSetResolveRetry(id, 2));
	printf("  SetConnectTimeOut: %08x\n", sceHttpSetConnectTimeOut(id, 1000000));
	printf("  SetSendTimeOut: %08x\n", sceHttpSetSendTimeOut(id, 1000000));
	printf("  SetRecvTimeOut: %08x\n", sceHttpSetRecvTimeOut(id, 1000000));
	printf("  AddExtraHeader: %08x\n", sceHttpAddExtraHeader(id, (char *)"X-Test", (char *)"1", 0));
	printf("  DeleteHeader: %08x\n", sceHttpDeleteHeader(id, "X-Test"));
	printf("  DeleteHeader again: %08x\n", sceHttpDeleteHeader(id, "X-Test"));
	printf("  GetStatusCode: %08x\n", sceHttpGetStatusCode(id, &status));
	printf("  GetContentLength: %08x\n", sceHttpGetContentLength(id, &length));
	printf("  GetAllHeader: %08x\n", sceHttpGetAllHeader(id, &header, &headerSize));
	printf("  ReadData: %08x\n", sceHttpReadData(id, data, sizeof(data)));

	// Whatever these create is deleted again, so the ids the rest of the test sees don't move.
	int conn = createConnection(id);
	printf("  CreateConnection: %08x\n", conn);
	if (conn > 0) {
		sceHttpDeleteConnection(conn);
	}
	int req = createRequest(id);
	printf("  CreateRequest: %08x\n", req);
	if (req > 0) {
		sceHttpDeleteRequest(req);
	}
}

extern "C" int main(int argc, char *argv[]) {
	loadModules();
	printf("sceNetInit: %08x\n", sceNetInit(65536, 30, 0x1000, 30, 0x1000));
	printf("sceHttpInit: %08x\n", sceHttpInit(0x40000));

	printf("\nTemplates:\n");
	int t[4];
	for (int i = 0; i < 4; ++i) {
		t[i] = createTemplate();
	}
	printf("  created: %08x %08x %08x %08x\n", t[0], t[1], t[2], t[3]);
	printf("  delete second: %08x\n", sceHttpDeleteTemplate(t[1]));
	printf("  delete second again: %08x\n", sceHttpDeleteTemplate(t[1]));
	printf("  probe: %s %s %s %s\n", probe(t[0]), probe(t[1]), probe(t[2]), probe(t[3]));
	testCalls("  Deleted template", t[1]);
	printf("  delete fourth: %08x\n", sceHttpDeleteTemplate(t[3]));
	int tNew1 = createTemplate();
	int tNew2 = createTemplate();
	int tNew3 = createTemplate();
	printf("  created after deletes: %08x %08x %08x\n", tNew1, tNew2, tNew3);
	sceHttpDeleteTemplate(tNew3);
		sceHttpDeleteTemplate(tNew1);
	sceHttpDeleteTemplate(tNew2);

	printf("\nConnections:\n");
	int c[4];
	c[0] = createConnection(t[0]);
	c[1] = createConnection(t[0]);
	c[2] = createConnection(t[2]);
	c[3] = createConnection(t[2]);
	printf("  created: %08x %08x %08x %08x\n", c[0], c[1], c[2], c[3]);
	printf("  delete second: %08x\n", sceHttpDeleteConnection(c[1]));
	printf("  delete second again: %08x\n", sceHttpDeleteConnection(c[1]));
	printf("  probe: %s %s %s %s\n", probe(c[0]), probe(c[1]), probe(c[2]), probe(c[3]));
	testCalls("  Deleted connection", c[1]);
	printf("  delete fourth: %08x\n", sceHttpDeleteConnection(c[3]));
	int cNew1 = createConnection(t[2]);
	int cNew2 = createConnection(t[2]);
	int cNew3 = createConnection(t[2]);
	printf("  created after deletes: %08x %08x %08x\n", cNew1, cNew2, cNew3);
	sceHttpDeleteConnection(cNew3);
	sceHttpDeleteConnection(cNew1);
	sceHttpDeleteConnection(cNew2);
	int cu = sceHttpCreateConnectionWithURL(t[0], "http://localhost/", 0);
	printf("  WithURL: %08x\n", cu);

	printf("\nRequests:\n");
	int r[4];
	r[0] = createRequest(c[0]);
	r[1] = createRequest(c[0]);
	r[2] = createRequest(c[2]);
	r[3] = createRequest(c[2]);
	printf("  created: %08x %08x %08x %08x\n", r[0], r[1], r[2], r[3]);
	printf("  delete second: %08x\n", sceHttpDeleteRequest(r[1]));
	printf("  delete second again: %08x\n", sceHttpDeleteRequest(r[1]));
	printf("  probe: %s %s %s %s\n", probe(r[0]), probe(r[1]), probe(r[2]), probe(r[3]));
	testCalls("  Deleted request", r[1]);
	printf("  delete fourth: %08x\n", sceHttpDeleteRequest(r[3]));
	int rNew1 = createRequest(c[2]);
	int rNew2 = createRequest(c[2]);
	int rNew3 = createRequest(c[2]);
	printf("  created after deletes: %08x %08x %08x\n", rNew1, rNew2, rNew3);
	sceHttpDeleteRequest(rNew3);
	sceHttpDeleteRequest(rNew1);
	sceHttpDeleteRequest(rNew2);
	int ru = sceHttpCreateRequestWithURL(cu, PSP_HTTP_METHOD_GET, (char *)"http://localhost/a", 0);
	printf("  WithURL: %08x\n", ru);
	const int methods[] = { 3, 4, 0x100, -1 };
	for (size_t i = 0; i < ARRAY_SIZE(methods); ++i) {
		int req = sceHttpCreateRequest(c[0], (PspHttpMethod)methods[i], (char *)"/", 0);
		printf("  method %d: %08x\n", methods[i], req);
		if (req > 0) {
			sceHttpDeleteRequest(req);
		}
	}

	printf("\n");
	testCalls("Template", t[0]);
	testCalls("Connection", c[0]);
	testCalls("Request", r[0]);
	testCalls("Id 0", 0);
	testCalls("Id -1", -1);
	testCalls("Unused template", 0x00000010);
	testCalls("Unused connection", 0x01000010);
	testCalls("Unused request", 0x02000010);
	testCalls("Template number with type 3", 0x03000000 | t[0]);
	testCalls("Template number with type 0x80", 0x80000000 | t[0]);

	printf("\nDelete with the wrong call:\n");
	printf("  DeleteConnection(request): %08x\n", sceHttpDeleteConnection(r[2]));
	printf("  DeleteTemplate(request): %08x\n", sceHttpDeleteTemplate(r[2]));
	printf("  DeleteRequest(connection): %08x\n", sceHttpDeleteRequest(c[2]));
	printf("  DeleteTemplate(connection): %08x\n", sceHttpDeleteTemplate(c[2]));
	printf("  DeleteRequest(template): %08x\n", sceHttpDeleteRequest(t[2]));
	printf("  DeleteConnection(template): %08x\n", sceHttpDeleteConnection(t[2]));
	printf("  probe: %s %s %s\n", probe(r[2]), probe(c[2]), probe(t[2]));

	printf("\nDelete a connection that has a request:\n");
	printf("  DeleteConnection: %08x\n", sceHttpDeleteConnection(c[2]));
	printf("  probe: %s %s\n", probe(c[2]), probe(r[2]));
	printf("  DeleteRequest: %08x\n", sceHttpDeleteRequest(r[2]));
	printf("  DeleteConnection: %08x\n", sceHttpDeleteConnection(c[2]));

	printf("\nDelete a template that has a connection:\n");
	printf("  DeleteTemplate: %08x\n", sceHttpDeleteTemplate(t[0]));
	printf("  probe: %s %s %s\n", probe(t[0]), probe(c[0]), probe(r[0]));
	int rAfter = createRequest(c[0]);
	printf("  CreateRequest on its connection: %08x\n", rAfter);
	if (rAfter > 0) {
		sceHttpDeleteRequest(rAfter);
	}

	printf("\nCleanup:\n");
	printf("  DeleteRequest: %08x\n", sceHttpDeleteRequest(r[0]));
	printf("  DeleteRequest: %08x\n", sceHttpDeleteRequest(ru));
	printf("  DeleteConnection: %08x\n", sceHttpDeleteConnection(c[0]));
	printf("  DeleteConnection: %08x\n", sceHttpDeleteConnection(cu));
	printf("  DeleteTemplate: %08x\n", sceHttpDeleteTemplate(t[2]));
	printf("  sceHttpEnd: %08x\n", sceHttpEnd());
	printf("  sceNetTerm: %08x\n", sceNetTerm());
	unloadModules();
	return 0;
}

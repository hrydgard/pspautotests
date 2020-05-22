#include <common.h>
#include <pspge.h>
#include <pspthreadman.h>
#include <psputils.h>

#include "../commands/commands.h"

extern "C" {
#include "sysmem-imports.h"
}

struct SceGeStack {
	unsigned int stack[8];
};

struct PspGeListArgs2 {
	unsigned int size;
	PspGeContext *context;
	u32 numStacks;
	SceGeStack *stacks;
};

static u32 __attribute__((aligned(16))) list[131072];

void testListParam(const char *title, PspGeListArgs2 *args, bool log = true) {
	PspGeContext ctx;
	int listID = sceGeListEnQueue(list, list + 1, -1, (PspGeListArgs *)args);
	if (listID >= 0) {
		if (log) {
			checkpoint("%s: OK", title);
		}
	} else {
		checkpoint("%s: %08x", title, listID);
	}
	sceGeBreak(1, NULL);
}

void testListParamHead(const char *title, PspGeListArgs2 *args, bool log = true) {
	PspGeContext ctx;
	int listID = sceGeListEnQueueHead(list, list + 1, -1, (PspGeListArgs *)args);
	sceGeContinue();
	if (listID >= 0) {
		if (log) {
			checkpoint("%s: OK", title);
		}
	} else {
		checkpoint("%s (Head): %08x", title, listID);
	}
	sceGeBreak(1, NULL);
}

extern "C" int main(int argc, char *argv[]) {
	memset(list, 0, sizeof(list));
	sceKernelDcacheWritebackRange(list, sizeof(list));

	PspGeListArgs2 args = { 0 };
	PspGeContext ctx;

	checkpointNext("Normal:");
	memset(&args, 0, sizeof(args));
	args.size = sizeof(args);
	testListParam("  Normal", &args);
	testListParamHead("  Normal", &args);

	checkpointNext("Sizes:");
	memset(&args, 0, sizeof(args));
	static const int sizes[] = { -1, 0, 1, 4, 5, 8, 12, 15, 16, 17, 32, 64, 1024 };
	for (int i = 0; i < ARRAY_SIZE(sizes); ++i) {
		args.size = sizes[i];
		char temp[256];
		snprintf(temp, sizeof(temp), "  Size=%d", sizes[i]);
		testListParam(temp, &args);
		testListParamHead(temp, &args);
	}

	checkpointNext("Context:");
	memset(&args, 0, sizeof(args));
	static const int ctx_sizes[] = { -1, 0, 4, 8, 12, 16 };
	for (int i = 0; i < ARRAY_SIZE(ctx_sizes); ++i) {
		memset(&ctx, 0xCC, sizeof(ctx));
		args.size = ctx_sizes[i];
		args.context = &ctx;
		testListParam("  With context", &args, false);
		checkpoint("  Context size=%d: %s", ctx_sizes[i], ctx.context[26] == 0xCCCCCCCC ? "not used" : "used");

		memset(&ctx, 0xCC, sizeof(ctx));
		testListParamHead("  With context", &args, false);
		checkpoint("  Context size=%d (Head): %s", ctx_sizes[i], ctx.context[26] == 0xCCCCCCCC ? "not used" : "used");
	}

	checkpointNext("Stack depths:");
	memset(&args, 0, sizeof(args));
	static const int stack_depths[] = { -1, 0, 1, 4, 5, 255, 256, 257, 1024 };
	for (int i = 0; i < ARRAY_SIZE(stack_depths); ++i) {
		args.size = sizeof(args);
		// Note that stacks is NULL in this case.
		args.numStacks = stack_depths[i];
		char temp[256];
		snprintf(temp, sizeof(temp), "  Stack depth=%d", stack_depths[i]);
		testListParam(temp, &args);
		testListParamHead(temp, &args);
	}

	checkpointNext("Stack depth by size:");
	static const int stack_sizes[] = { -1, 0, 4, 8, 12, 15, 16, 17, 32 };
	memset(&args, 0, sizeof(args));
	for (int i = 0; i < ARRAY_SIZE(stack_sizes); ++i) {
		args.size = stack_sizes[i];
		args.numStacks = 256;
		char temp[256];
		snprintf(temp, sizeof(temp), "  Stack=256 size=%d", stack_sizes[i]);
		testListParam(temp, &args);
		testListParamHead(temp, &args);
	}

	return 0;
}
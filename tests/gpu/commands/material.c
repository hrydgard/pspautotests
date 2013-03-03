#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <common.h>
#include <pspkernel.h>

#define BUF_WIDTH 512
#define SCR_WIDTH 480
#define SCR_HEIGHT 272

unsigned int __attribute__((aligned(16))) list[262144];
unsigned int __attribute__((aligned(16))) clut[] = { 0xaaaaaaaa, 0xffffffff, 0x00000000 };
unsigned char __attribute__((aligned(16))) imageData[] = {
	0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

typedef struct
{
	unsigned short a, b;
	unsigned short x, y, z;
} Vertex;

typedef struct
{
	unsigned short a, b;
	unsigned long color;
	unsigned short x, y, z;
} VertexColor;

Vertex vertices[2] = { {0, 0, 10, 10, 0}, {4, 4, 470, 262, 0} };
VertexColor colorVertices[2] = { {0, 0, 0x77333388, 10, 10, 0}, {4, 4, 0x77338833, 470, 262, 0} };

void nextBox(int *x, int *y)
{
	vertices[0].x = *x;
	vertices[0].y = *y;
	vertices[1].x = *x + 40;
	vertices[1].y = *y + 20;
	*x += 47;
	if (*x >= 470) {
		*x = 10;
		*y += 26;
	}
	sceKernelDcacheWritebackRange(vertices, sizeof(vertices));

	sceGuEnable(GU_TEXTURE_2D);
	sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
	sceGuClutLoad(1, clut);
	sceGuTexMode(GU_PSM_T8, 0, 0, GU_FALSE);
	sceGuTexFunc(GU_TFX_DECAL, GU_TCC_RGBA);
	sceGuTexImage(0, 4, 4, 16, imageData);

	sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, vertices);
}

void nextBoxHasColor(int *x, int *y)
{
	colorVertices[0].x = *x;
	colorVertices[0].y = *y;
	colorVertices[1].x = *x + 40;
	colorVertices[1].y = *y + 20;
	*x += 47;
	if (*x >= 470) {
		*x = 10;
		*y += 26;
	}
	sceKernelDcacheWritebackRange(colorVertices, sizeof(colorVertices));

	sceGuEnable(GU_TEXTURE_2D);
	sceGuTexMode(GU_PSM_T8, 0, 0, GU_FALSE);
	sceGuTexFunc(GU_TFX_DECAL, GU_TCC_RGBA);
	sceGuTexImage(0, 4, 4, 16, imageData);

	sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, NULL, colorVertices);
}

void draw()
{
	int x = 10, y = 10;

	sceDisplaySetMode(0, SCR_WIDTH, SCR_HEIGHT);
	sceGuStart(GU_DIRECT, list);
	sceGuClear(GU_COLOR_BUFFER_BIT);

	sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
	sceGuClutLoad(1, clut);
	
	sceGuSendCommandi(0x53, 0);
	sceGuSendCommandi(0x55, 0);
	nextBox(&x, &y);

	sceGuSendCommandi(0x53, 0xFF);
	sceGuSendCommandi(0x55, 0xFF0000);
	nextBox(&x, &y);

	sceGuSendCommandi(0x55, 0x00FF00);
	nextBox(&x, &y);
	
	sceGuSendCommandi(0x53, 0);
	sceGuSendCommandi(0x55, 0x0000FF);
	nextBox(&x, &y);
	
	sceGuSendCommandi(0x53, 0);
	sceGuSendCommandi(0x55, 0);
	nextBoxHasColor(&x, &y);

	sceGuSendCommandi(0x53, 0x1);
	sceGuSendCommandi(0x58, 0x0);
	sceGuSendCommandi(0x55, 0xFF0000);
	sceGuSendCommandi(0x5C, 0x0000FF);
	sceGuSendCommandi(0x5D, 0x0);
	sceGuSendCommandi(0x17, 1);
	nextBoxHasColor(&x, &y);

	/*ScePspFVector3 pos = {x, y, 0};
	sceGuLight(0, GU_DIRECTIONAL, GU_AMBIENT_AND_DIFFUSE, &pos);
	nextBox(&x, &y);

	pos.x = x; pos.y = y;
	sceGuLight(0, GU_DIRECTIONAL, GU_AMBIENT_AND_DIFFUSE, &pos);
	sceGuLightMode(GU_SINGLE_COLOR);
	sceGuLightColor(0, GU_AMBIENT, 0x00FF00);
	sceGuLightAtt(0, 1.0, 1.0, 1.0);
	nextBox(&x, &y);*/

	sceGuFinish();
	sceGuSync(GU_SYNC_LIST, GU_SYNC_WHAT_DONE);
	sceGuSync(0, 0);

	sceDisplayWaitVblankStart();
}

void init()
{
	void *fbp0 = 0;
 
	sceGuInit();
	sceGuStart(GU_DIRECT, list);
	sceGuDrawBuffer(GU_PSM_8888, fbp0, BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, fbp0, BUF_WIDTH);
	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFinish();
	sceGuSync(0, 0);
 
	sceDisplayWaitVblankStart();
	sceGuDisplay(1);
}

int main(int argc, char *argv[])
{
	int i;
	init();
	for (i = 0; i < 120; ++i) {
		draw();
	}

	emulatorEmitScreenshot();

	sceGuTerm();

	return 0;
}
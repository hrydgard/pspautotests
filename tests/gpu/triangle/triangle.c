#define GRAPHIC_TEST 1

#include <common.h>
#include <vram.h>
 
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspdebug.h>
#include <stdlib.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>

static unsigned int __attribute__((aligned(16))) list[262144];
 
#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)

struct Vertex
{
   unsigned int color;
   float x, y, z;
};

struct Vertex __attribute__((aligned(16))) vertices[1*3] =
{
   {0xFF0000FF, 0.0f, -50.0f, 0.0f}, // Top, red
   {0xFF00FF00, 50.0f, 50.0f, 0.0f}, // Right, green
   {0xFFFF0000, -50.0f, 50.0f, 0.0f}, // Left, blue
};
int main(int argc, char* argv[])
{
	pspDebugScreenInit();
 
	// Setup GU
 
	void* fbp0 = getStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_8888);
	void* fbp1 = getStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_8888);
	void* zbp = getStaticVramBuffer(BUF_WIDTH,SCR_HEIGHT,GU_PSM_4444);
 
	sceGuInit();
 
	sceGuStart(GU_DIRECT,list);
	sceGuDrawBuffer(GU_PSM_8888,fbp0,BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH,SCR_HEIGHT,fbp1,BUF_WIDTH);
	sceGuDepthBuffer(zbp,BUF_WIDTH);
	sceGuOffset(2048 - (SCR_WIDTH/2),2048 - (SCR_HEIGHT/2));
	sceGuViewport(2048,2048,SCR_WIDTH,SCR_HEIGHT);
	sceGuDepthRange(65535,0);
	sceGuScissor(0,0,SCR_WIDTH,SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuDisable(GU_TEXTURE_2D);
	sceGuFinish();
	sceGuSync(0,0);
 
	sceDisplayWaitVblankStart();
	sceGuDisplay(1);

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
	
	ScePspFVector3 pos = {240.0f, 136.0f, 0.0f};

	int val = 30;

	{
		SceCtrlData pad;
 
		sceGuStart(GU_DIRECT,list);
 
		sceGuClearColor(0);
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);

		sceCtrlPeekBufferPositive(&pad, 1);

		sceGumMatrixMode(GU_PROJECTION);
		sceGumLoadIdentity();
		sceGumOrtho(0, 480, 272, 0, -1, 1);

		sceGumMatrixMode(GU_VIEW);
		sceGumLoadIdentity();
 
		sceGumMatrixMode(GU_MODEL);
		sceGumLoadIdentity();

		{
			// Draw triangle
			
			sceGumTranslate(&pos);
			sceGumRotateZ(val*0.03f);

			sceGumDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_3D,1*3,0,vertices);
		}

		sceGuFinish();
		sceGuSync(0,0);

		pspDebugScreenSetOffset((int)fbp0);
		pspDebugScreenSetXY(0,0);

		pspDebugScreenPrintf("x: %.2f y: %.2f z: %.2f",pos.x,pos.y,pos.z);

		sceDisplayWaitVblankStart();
		fbp0 = sceGuSwapBuffers();

		//val++;
	}
 
	sceGuTerm();
	emulatorEmitScreenshot();

	//sceKernelExitGame();

	return 0;
}
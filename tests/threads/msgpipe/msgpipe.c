#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

typedef struct {
	int value1;
	int value2;
	int index;
} Message;

void testMsgPipeSimple1() {
	Message send_message1 = {1, 2, -1};
	Message recv_message1 = {0};
	int msgpipe;
	int result;
	int n;

	msgpipe = sceKernelCreateMsgPipe("MSGPIPE", 2, 0, sizeof(Message) * 3, NULL);
	//msgpipe = sceKernelCreateMsgPipe("MSGPIPE", 2, 0, 1, NULL);
	printf("CREATE:%08X\n", msgpipe);
	for (n = 0; n < 3; n++)
	{
		int resultSize;
		send_message1.index = n;
		result = sceKernelSendMsgPipe(msgpipe, &send_message1, sizeof(Message), 0, &resultSize, NULL);
		printf("SEND[%d]:%08X, %08X, %d, %d, %d\n", n, result, resultSize, send_message1.value1, send_message1.value2, send_message1.index);
	}
	for (n = 0; n < 3; n++)
	{
		int resultSize;
		result = sceKernelReceiveMsgPipe(msgpipe, &recv_message1, sizeof(Message), 0, &resultSize, NULL);
		printf("RECV[%d]:%08X, %08X, %d, %d, %d\n", n, result, resultSize, recv_message1.value1, recv_message1.value2, recv_message1.index);
	}
	printf("DELETE:%08X\n", sceKernelDeleteMsgPipe(msgpipe));
	printf("DELETE2:%08X\n", sceKernelDeleteMsgPipe(msgpipe));

}

int main(int argc, char **argv) {
	testMsgPipeSimple1();
	return 0;
}
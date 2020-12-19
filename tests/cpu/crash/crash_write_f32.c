#include <common.h>
#include <pspkernel.h>

#include <stdint.h>

int main(int argc, char *argv[]) {
	volatile float *bad_address = (float *)0xAC;
	*bad_address = 1337.0f;
	return 0;
}
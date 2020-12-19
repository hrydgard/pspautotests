#include <common.h>
#include <pspkernel.h>

#include <stdint.h>

int main(int argc, char *argv[]) {
	volatile float *bad_address = (float *)0xAC;
	float value = *bad_address;
	return 0;
}
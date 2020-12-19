#include <common.h>
#include <pspkernel.h>

#include <stdint.h>

int main(int argc, char *argv[]) {
	volatile uint32_t *bad_address = (uint32_t *)0xAC;

	uint32_t value = *bad_address;
	printf("%d", (int)value);

	return 0;
}
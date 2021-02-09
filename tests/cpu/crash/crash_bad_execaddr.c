#include <common.h>
#include <pspkernel.h>

#include <stdint.h>

typedef void (*func_ptr)();

int main(int argc, char *argv[]) {
	volatile func_ptr bad_address = (func_ptr)0xAC;
	bad_address();
	return 0;
}
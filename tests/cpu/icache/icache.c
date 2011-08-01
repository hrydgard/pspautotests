/**
 * Dynamic recompilation implementations may reuse cached functions.
 * This tests that if the code memory changed, the code must be updated.
 */
#include <common.h>

#include <pspkernel.h>
#include <pspthreadman.h>
#include <string.h>

char test_dyna[1024];

void test1() { printf("%d\n", 1); } void test1_end() { }
void test2() { printf("%d\n", 2); } void test2_end() { }

int main() {
	memcpy(test_dyna, test1, test1_end - test1); ((void(*)(void))test_dyna)();
	memcpy(test_dyna, test2, test2_end - test2); ((void(*)(void))test_dyna)();
	
	return 0;
}
#include <pspkernel.h>
#include <oslib/oslib.h>

PSP_MODULE_INFO("dmac test", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);

char dataSource[] = "Hello World. This is a test to check if sceDmacMemcpy works.";
char dataDest[sizeof(dataSource)] = {0};

int main(int argc, char *argv[]) {
	Kprintf("%s\n", dataSource);
	Kprintf("%s\n", dataDest);

	oslUncacheData(dataSource, sizeof(dataSource));
	oslUncacheData(dataDest, sizeof(dataDest));
	sceDmacMemcpy(dataDest, dataSource, sizeof(dataSource));

	Kprintf("%s\n", dataDest);

	void *ptr;
	ptr = memalign(128 , 2048); Kprintf("%d\n", ((int)ptr) % 128);
	ptr = memalign(1024, 2048); Kprintf("%d\n", ((int)ptr) % 1024);
	//ptr = memalign(100 , 2048); Kprintf("%d\n", ((int)ptr) % 100);

	//Kprintf("%i bytes available\n", oslGetRamStatus().maxAvailable);

	return 0;
}
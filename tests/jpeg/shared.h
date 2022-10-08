#include <common.h>
#include <pspjpeg.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <string.h>

struct File {
	File() {
		size = 32;
		data = new uint8_t[size];
		memset(data, 0, size);
		cleanup = true;
	}

	File(bool makenull, int s) {
		data = NULL;
		size = s;
		cleanup = false;
	}

	File(const File &other, int s) {
		data = other.data;
		size = s;
		cleanup = false;
	}

	File(const char *filename) {
		SceUID fd = sceIoOpen(filename, PSP_O_RDONLY, 0644);
		if (fd < 0) {
			printf("TESTERROR: Cannot load %s\n", filename);
			data = NULL;
			size = 0;
			cleanup = false;
			return;
		}

		size = sceIoLseek32(fd, 0, PSP_SEEK_END);
		sceIoLseek32(fd, 0, PSP_SEEK_SET);

		data = new uint8_t[size];
		sceIoRead(fd, data, size);
		sceIoClose(fd);
		cleanup = true;
	}

	~File() {
		if (cleanup) {
			delete[] data;
		}
	}

	uint8_t *data;
	int size;
	bool cleanup;
};

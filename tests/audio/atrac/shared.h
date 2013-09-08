#include <common.h>
#include "atrac.h"

void LoadAtrac();
void UnloadAtrac();

struct Atrac3File {
	Atrac3File(const char *filename);
	~Atrac3File();

	void Reload(const char *filename);
	void Require();

	bool IsValid() {
		return data_ != NULL;
	}
	void *Data() {
		return data_;
	}
	size_t Size() {
		return size_;
	}

private:
	size_t size_;
	void *data_;
};

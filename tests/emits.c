void emitInt(int v) {
	asm("syscall 0x1010");
}

void emitFloat(float v) {
	asm("syscall 0x1011");
}

void emitString(char *v) {
	asm("syscall 0x1012");
}

void emitComment(char *v) {
	asm("syscall 0x1012");
}

void emitMemoryBlock(void *address, unsigned int size) {
	asm("syscall 0x1013");
}

void emitHex(void *address, unsigned int size) {
	asm("syscall 0x1014");
}

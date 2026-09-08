.set noreorder

#include "pspimport.s"

# ThreadManForKernel doesn't export the Tlspl calls - importing them from there links fine but
# every call comes back 8002013A, library not yet linked. ThreadManForUser does export them, and
# a kernel module can import it; the library that actually can't be imported from kernel mode is
# SysMemUserForUser, which is why common.c leaves the sdkver helpers out of a kernel build.
IMPORT_START "ThreadManForUser",0x40010000
IMPORT_FUNC	"ThreadManForUser",0x8daff657,sceKernelCreateTlspl
IMPORT_FUNC	"ThreadManForUser",0x32BF938E,sceKernelDeleteTlspl

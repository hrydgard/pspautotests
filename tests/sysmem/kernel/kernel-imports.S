.set noreorder

#include "pspimport.s"

# A kernel module can't import SysMemUserForUser, so the partition and heap calls come from the
# kernel library instead. The Vpl/Fpl/MsgPipe calls stay on ThreadManForUser, which a kernel
# module can import - and that's the point of the sweep, since going through the ordinary user
# NID is exactly the case where the caller's privilege is all that distinguishes it.
IMPORT_START "SysMemForKernel",0x40010000
IMPORT_FUNC	"SysMemForKernel",0x237DBD4F,sceKernelAllocPartitionMemory
IMPORT_FUNC	"SysMemForKernel",0xB6D61D02,sceKernelFreePartitionMemory
IMPORT_FUNC	"SysMemForKernel",0x1C1FBFE7,sceKernelCreateHeap
IMPORT_FUNC	"SysMemForKernel",0xC9805775,sceKernelDeleteHeap
IMPORT_FUNC	"SysMemForKernel",0x636C953B,sceKernelAllocHeapMemory
IMPORT_FUNC	"SysMemForKernel",0x7B749390,sceKernelFreeHeapMemory
IMPORT_FUNC	"SysMemForKernel",0xEB7A74DB,sceKernelAllocHeapMemoryWithOption
IMPORT_FUNC	"SysMemForKernel",0x9697CD32,sceKernelPartitionTotalFreeMemSize

IMPORT_START "ThreadManForUser",0x40010000
IMPORT_FUNC	"ThreadManForUser",0x56C039B5,sceKernelCreateVpl
IMPORT_FUNC	"ThreadManForUser",0x89B3D48C,sceKernelDeleteVpl
IMPORT_FUNC	"ThreadManForUser",0xC07BB470,sceKernelCreateFpl
IMPORT_FUNC	"ThreadManForUser",0xED1410E0,sceKernelDeleteFpl
IMPORT_FUNC	"ThreadManForUser",0x7C0DC2A0,sceKernelCreateMsgPipe
IMPORT_FUNC	"ThreadManForUser",0xF0B7DA1C,sceKernelDeleteMsgPipe

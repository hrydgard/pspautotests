#include <common.h>

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspthreadman.h>
#include <psploadexec.h>

inline void schedfVTimer(SceUID vtimer) {
	SceKernelVTimerInfo info;
	if (vtimer >= 0) {
		int result = sceKernelReferVTimerStatus(vtimer, &info);
		if (result >= 0) {
			u64 base = *(u64 *)&info.base;
			u64 current = *(u64 *)&info.current;
			u64 schedule = *(u64 *)&info.schedule;
			schedf("VTimer: (size=%d,name=%s,active=%d,handler=%d,common=%08x,base=%d,current=+%lld,schedule=+%lld)\n", info.size, info.name, info.active, info.handler == 0 ? 0 : 1, info.common, base == 0 ? 0 : 1, current - base, schedule - base);
		} else {
			schedf("VTimer: Invalid (%08x)\n", result);
		}
	} else {
		schedf("VTimer: Failed (%08x)\n", vtimer);
	}
}

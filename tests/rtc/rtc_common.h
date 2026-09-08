#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define sceRtcSetWin32FileTime sceRtcSetWin32FileTime_WRONG

#include <pspkernel.h>
#include <psprtc.h>

#include <limits.h>

#undef sceRtcSetWin32FileTime

int sceRtcSetWin32FileTime(ScePspDateTime *date, u64 filetime);

#include <inttypes.h>
// These are not in the pspsdk
int sceRtcSetTime64_t(ScePspDateTime *date, uint64_t time);
int sceRtcGetTime64_t(const ScePspDateTime *date, uint64_t *time);

static void DumpPSPTimeOnly(const ScePspDateTime *pt) {
	printf("%d, %d, %d, %d, %d, %d, %d", pt->year, pt->month, pt->day, pt->hour, pt->minute, pt->second, (int)pt->microsecond);
}

static void DumpPSPTime(const char *name, const ScePspDateTime *pt) {
	printf("%s ", name);
	DumpPSPTimeOnly(pt);
	printf("\n");
}

static void DumpTick(const char* name, u64 ticks)
{
	ScePspDateTime pt;
	printf("%s %llu\n", name, ticks);
	sceRtcSetTick(&pt, &ticks);
	DumpPSPTime("",&pt);
}

static void FillPSPTime(ScePspDateTime* pt, int year, int month, int day, int hour, int min, int sec, int micro)
{
	pt->year = year;
	pt->month = month;
	pt->day = day;
	pt->hour = hour;
	pt->minute = min;
	pt->second = sec;
	pt->microsecond = micro;
}

static void checkPspTime(ScePspDateTime pt) {
	if (pt.year > 1980) {
		printf("Year: OK\n");
	} else {
		printf("Year: Failed, or great job on that time machine to %d\n", pt.year);
	}

	if (pt.month < 1 || pt.month > 12 || pt.day < 1 || pt.day > 31) {
		printf("Date: Failed %04d-%02d-%02d\n", pt.year, pt.month, pt.day);
	} else {
		printf("Date: OK\n");
	}

	if (pt.hour < 0 || pt.hour > 23 || pt.minute < 0 || pt.minute > 59 || pt.second < 0 || pt.second > 59) {
		printf("Time: Failed %02d:%02d:%02d\n", pt.hour, pt.minute, pt.second);
	} else {
		printf("Time: OK\n");
	}

	if (pt.microsecond >= 1000000) {
		printf("Microseconds: Failed, impossibly high: %d\n", (int)pt.microsecond);
	} else {
		printf("Microseconds: OK\n");
	}
}

#ifdef __cplusplus
}
#endif

#include <common.h>

#include <pspkernel.h>
#include <psprtc.h>
#include <limits.h>



/**
 * Check that getCurrentTick works fine.
 *
 * @TODO: Currently sceKernelDelayThread only work with ms.
 *        It should check that work with microseconds precission.
 */
void checkGetCurrentTick() {
	u64 tick0, tick1;
	int microseconds = 2 * 1000; // 2ms

	printf("Checking sceRtcGetCurrentTick\n");

	sceRtcGetCurrentTick(&tick0);
	{
		sceKernelDelayThread(microseconds);
	}
	sceRtcGetCurrentTick(&tick1);
	
	printf("%d\n", (tick1 - tick0) >= microseconds);
}

void checkPspTime(pspTime pt) {
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

	if (pt.hour < 0 || pt.hour > 23 || pt.minutes < 0 || pt.minutes > 59 || pt.seconds < 0 || pt.seconds > 59) {
		printf("Time: Failed %02d:%02d:%02d\n", pt.hour, pt.minutes, pt.seconds);
	} else {
		printf("Time: OK\n");
	}

	if (pt.microseconds >= 1000000) {
		printf("Microseconds: Failed, impossibly high: %d\n", pt.microseconds);
	} else {
		printf("Microseconds: OK\n");
	}
}

void checkGetCurrentClock() {
	printf("Checking sceRtcGetCurrentClock\n");

	pspTime pt_baseline;
	pspTime pt;

	do
	{
		sceRtcGetCurrentClock(&pt_baseline, 0);
		sceRtcGetCurrentClock(&pt, -60);
	}
	// Rollover is annoying.  We could test in a more complicated way, I guess.
	while (pt_baseline.minutes == 59 && pt_baseline.seconds == 59);

	if (pt.hour != pt_baseline.hour - 1 && !(pt.hour == 12 && pt_baseline.hour == 1))
		printf("-60 TZ: Failed, got time different by %d hours.\n", (pt_baseline.hour - pt.hour) % 12);
	else
		printf("-60 TZ: OK\n");

	checkPspTime(pt);

	// Crash.
	//printf("NULL, 0 TZ: %08x\n", sceRtcGetCurrentClock(NULL, 0));
	printf("0 TZ: %08x\n", sceRtcGetCurrentClock(&pt, 0));
	printf("+13 TZ: %08x\n", sceRtcGetCurrentClock(&pt, 13));
	printf("+60 TZ: %08x\n", sceRtcGetCurrentClock(&pt, 60));
	printf("-60 TZ: %08x\n", sceRtcGetCurrentClock(&pt, -60));
	printf("-600000 TZ: %08x\n", sceRtcGetCurrentClock(&pt, -600000));
	printf("INT_MAX TZ: %08x\n", sceRtcGetCurrentClock(&pt, INT_MAX));
	printf("-INT_MAX TZ: %08x\n", sceRtcGetCurrentClock(&pt, -INT_MAX));
}

void checkGetCurrentClockLocalTime() {
	printf("Checking sceRtcGetCurrentClockLocalTime\n");
	pspTime pt;

	// Crash.
	//printf("NULL: %08x\n", sceRtcGetCurrentClockLocalTime(NULL));
	// Not much to test here...
	printf("Normal: %08x\n", sceRtcGetCurrentClockLocalTime(&pt));

	checkPspTime(pt);
}

void checkDaysInMonth() {
	printf("Checking sceRtcGetDaysInMonth\n");
	printf("sceRtcGetDaysInMonth:2010, 4\n");
	printf("%d\n", sceRtcGetDaysInMonth(2010, 4));
}

void checkDayOfWeek() {
	printf("Checking sceRtcGetDayOfWeek\n");
	printf("sceRtcGetDayOfWeek:2010, 4, 27\n");
	printf("%d\n", sceRtcGetDayOfWeek(2010, 4, 27));
}

void checkSetTick()
{
	pspTime pt;
	u64 ticks = 835072;

	memset(&pt, 0, sizeof(pt));

	printf("checkSetTick: empty small value: %08x\n", sceRtcSetTick(&pt, &ticks));
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

	ticks = 62135596800000000L;
	memset(&pt, 0, sizeof(pt));
	printf("checkSetTick: empty rtcMagicOffset: %08x\n", sceRtcSetTick(&pt, &ticks));
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

	pt.year = 2012;
	pt.month = 9;
	pt.day = 20;
	pt.hour = 7;
	pt.minutes = 12;
	pt.seconds = 15;
	pt.microseconds = 500;
	printf("Normal: %08x\n", sceRtcGetTick(&pt, &ticks)); // if this does depend on timezone the next bit might have differnt results

	printf("checkSetTick: 2012, 09, 20, 7, 12, 15, 500: %08x\n", sceRtcSetTick(&pt, &ticks));
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);


	pt.year = 2010;
	pt.month = 9;
	pt.day = 20;
	pt.hour = 7;
	pt.minutes = 12;
	pt.seconds = 15;
	pt.microseconds = 500;
	printf("preset\n");
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);
	printf("checkSetTick: not empty:%08x\n", sceRtcSetTick(&pt, &ticks));
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);
}

void checkGetTick() {
	pspTime pt;
	u64 ticks;

	printf("Checking sceRtcGetTick\n");

	pt.year = 2012;
	pt.month = 9;
	pt.day = 20;
	pt.hour = 7;
	pt.minutes = 0;
	pt.seconds = 0;
	pt.microseconds = 0;
	printf("Normal: %08x\n", sceRtcGetTick(&pt, &ticks));
	// TODO: Should ticks match?  Depends on timezone?

	pt.year = 0;
	pt.month = 324;
	pt.day = 99;
	pt.hour = -56;
	pt.minutes = 42;
	pt.seconds = 42;
	pt.microseconds = 42000000;
	printf("Bad date: %08x\n", sceRtcGetTick(&pt, &ticks));
}


void checkRtcTickAddTicks()
{
	printf("Checking sceRtcTickAddTicks\n");

	//62135596800000445 and -2000 years
	u64 sourceTick = 62135596800000445;
	u64 destTick = 0;
	pspTime pt;

	printf("62135596800000445 adding -62135596800000445 ticks:%llu\n", sceRtcTickAddTicks(&destTick, &sourceTick, -62135596800000445));
	printf("source tick %llu\n", sourceTick);
	sceRtcSetTick(&pt, &sourceTick);
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

	printf("dest tick %llu\n", destTick);
	sceRtcSetTick(&pt, &destTick);
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

	sourceTick = 62135596800000445;
	destTick = 0;

	printf("62135596800000445 adding +62135596800000445 ticks: %llu\n", sceRtcTickAddTicks(&destTick, &sourceTick, 62135596800000445));
	sceRtcSetTick(&pt, &sourceTick);
	printf("source tick %llu\n", sourceTick);
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

	printf("dest tick%llu\n", destTick);
	sceRtcSetTick(&pt, &destTick);
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

	sourceTick = 62135596800000445;
	destTick = 0;

	printf("62135596800000445 adding +621355968000 ticks: %llu\n", sceRtcTickAddTicks(&destTick, &sourceTick, 621355968000));
	printf("source tick %llu\n", sourceTick);
	sceRtcSetTick(&pt, &sourceTick);
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

	printf("dest tick %llu\n", destTick);
	sceRtcSetTick(&pt, &destTick);
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

}

void checkRtcTickAddYears()
{
	printf("Checking checkRtcTickAddYears\n");

	//62135596800000445 and -2000 years
	u64 sourceTick = 62135596800000445;
	u64 destTick = 0;
	pspTime pt;

	printf("62135596800000445 adding -2000 years: %08x\n", sceRtcTickAddYears(&destTick, &sourceTick, -2000));
	printf("source tick %d\n", sourceTick);
	sceRtcSetTick(&pt, &sourceTick);
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

	printf("dest tick %d\n", destTick);
	sceRtcSetTick(&pt, &destTick);
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

	sourceTick = 62135596800000445;
	destTick = 0;

	printf("62135596800000445 adding +2000 years: %08x\n", sceRtcTickAddYears(&destTick, &sourceTick, 2000));
	sceRtcSetTick(&pt, &sourceTick);
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

	printf("dest tick %d\n", destTick);
	sceRtcSetTick(&pt, &destTick);
	printf("%d, %d, %d, %d, %d, %d, %d\n", pt.year, pt.month, pt.day, pt.hour, pt.minutes, pt.seconds, pt.microseconds);

}

int main(int argc, char **argv) {
	checkGetCurrentTick();
	checkDaysInMonth();
	checkDayOfWeek();
	checkGetCurrentClock();
	checkGetCurrentClockLocalTime();
	checkGetTick();
	checkSetTick();	
	checkRtcTickAddTicks();
	checkRtcTickAddYears();

	return 0;
}

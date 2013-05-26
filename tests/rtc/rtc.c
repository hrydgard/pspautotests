#include <common.h>

#define sceRtcSetWin32FileTime sceRtcSetWin32FileTime_WRONG

#include <pspkernel.h>
#include <psprtc.h>
#include <limits.h>

#undef sceRtcSetWin32FileTime
int sceRtcSetWin32FileTime(pspTime *date, u64 filetime);

void DumpPSPTime(const char* name, const pspTime* pt)
{
	printf("%s %d, %d, %d, %d, %d, %d, %d\n", name, pt->year, pt->month, pt->day, pt->hour, pt->minutes, pt->seconds, pt->microseconds);
}

void DumpTick(const char* name, u64 ticks)
{
	pspTime pt;
	printf("%s %llu\n", name, ticks);
	sceRtcSetTick(&pt, &ticks);
	DumpPSPTime("",&pt);
}

void FillPSPTime(pspTime* pt, int year, int month, int day, int hour, int min, int sec, int micro)
{
	pt->year = year;
	pt->month = month;
	pt->day = day;
	pt->hour = hour;
	pt->minutes = min;
	pt->seconds = sec;
	pt->microseconds = micro;
}


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
	
	//A game does this: sceRtcGetDayOfWeek(166970016, 1024, 0)
	printf("sceRtcGetDayOfWeek:166970016, 1024, 0\n");
	printf("%d\n", sceRtcGetDayOfWeek(166970016, 1024, 0));
	
	// test random no valid date
	// leap year
	printf("sceRtcGetDayOfWeek:2000, 0, 0\n");
	printf("%d\n", sceRtcGetDayOfWeek(2000, 0, 0));
	printf("sceRtcGetDayOfWeek:2000, 1, 0\n");
	printf("%d\n", sceRtcGetDayOfWeek(2000, 1, 0));
	printf("sceRtcGetDayOfWeek:2000, 573, 0\n");
	printf("%d\n", sceRtcGetDayOfWeek(2000, 573, 0));
	printf("sceRtcGetDayOfWeek:2000, 1, 2458\n");
	printf("%d\n", sceRtcGetDayOfWeek(2000, 1, 2458));
	printf("sceRtcGetDayOfWeek:2000, 4587, 2458\n");
	printf("%d\n", sceRtcGetDayOfWeek(2000, 4587, 2458));
	
	// standard year
	printf("sceRtcGetDayOfWeek:2001, 0, 0\n");
	printf("%d\n", sceRtcGetDayOfWeek(2001, 0, 0));
	printf("sceRtcGetDayOfWeek:2001, 1, 0\n");
	printf("%d\n", sceRtcGetDayOfWeek(2001, 1, 0));
	printf("sceRtcGetDayOfWeek:2001, 573, 0\n");
	printf("%d\n", sceRtcGetDayOfWeek(2001, 573, 0));
	printf("sceRtcGetDayOfWeek:2001, 1, 2458\n");
	printf("%d\n", sceRtcGetDayOfWeek(2001, 1, 2458));
	printf("sceRtcGetDayOfWeek:2001, 4587, 2458\n");
	printf("%d\n", sceRtcGetDayOfWeek(2001, 4587, 2458));
}

void checkSetTick()
{
	pspTime pt;
	u64 ticks = 835072;

	memset(&pt, 0, sizeof(pt));

	printf("checkSetTick: empty small value: %08x\n", sceRtcSetTick(&pt, &ticks));
	DumpPSPTime("",&pt);

	ticks = 62135596800000000ULL;
	memset(&pt, 0, sizeof(pt));
	printf("checkSetTick: empty rtcMagicOffset: %08x\n", sceRtcSetTick(&pt, &ticks));
	DumpPSPTime("",&pt);

	FillPSPTime(&pt,2012,9,20,7,12,15,500);
	printf("Normal: %08x\n", sceRtcGetTick(&pt, &ticks)); // if this does depend on timezone the next bit might have differnt results
	printf("checkSetTick: 2012, 09, 20, 7, 12, 15, 500: %08x\n", sceRtcSetTick(&pt, &ticks));
	DumpPSPTime("",&pt);


	FillPSPTime(&pt,2010,9,20,7,12,15,500);
	printf("preset\n");
	DumpPSPTime("",&pt);
	printf("checkSetTick: not empty:%08x\n", sceRtcSetTick(&pt, &ticks));
	DumpPSPTime("",&pt);
}

void checkGetTick() {
	pspTime pt;
	u64 ticks;

	printf("Checking sceRtcGetTick\n");


	FillPSPTime(&pt,10,1,1,0,0,0,0);
	printf("Normal: %08x\n", sceRtcGetTick(&pt, &ticks));
	printf("Ticks : %llu\n",ticks);

	FillPSPTime(&pt,9998,1,1,0,0,0,0);
	printf("Bad date: %08x\n", sceRtcGetTick(&pt, &ticks));
	printf("Ticks : %llu\n",ticks);
	
	FillPSPTime(&pt,0,1,1,0,0,0,0);
	printf("Min year: %08x\n", sceRtcGetTick(&pt, &ticks));
	printf("Ticks : %llu\n",ticks);

	FillPSPTime(&pt,9999,1,1,0,0,0,0);
	printf("Max year: %08x\n", sceRtcGetTick(&pt, &ticks));
	printf("Ticks : %llu\n",ticks);
	
	FillPSPTime(&pt,10000,1,1,0,0,0,0);
	printf("Year overflow: %08x\n", sceRtcGetTick(&pt, &ticks));
	printf("Ticks : %llu\n",ticks);
}


void checkRtcTickAddTicks()
{
	printf("Checking sceRtcTickAddTicks\n");

	u64 sourceTick = 62135596800000445ULL;
	u64 destTick = 0;
	pspTime pt;

	printf("62135596800000445 adding -62135596800000445 ticks:%d\n", sceRtcTickAddTicks(&destTick, &sourceTick,(u64)-62135596800000445ULL));
	printf("source tick %llu\n", sourceTick);
	sceRtcSetTick(&pt, &sourceTick);
	DumpPSPTime("",&pt);

	printf("dest tick %llu\n", destTick);
	sceRtcSetTick(&pt, &destTick);
	DumpPSPTime("",&pt);

	sourceTick = 62135596800000445ULL;
	destTick = 0;

	printf("62135596800000445 adding +62135596800000445 ticks: %d\n", sceRtcTickAddTicks(&destTick, &sourceTick, sourceTick));
	sceRtcSetTick(&pt, &sourceTick);
	printf("source tick %llu\n", sourceTick);
	DumpPSPTime("",&pt);

	printf("dest tick%llu\n", destTick);
	sceRtcSetTick(&pt, &destTick);
	DumpPSPTime("",&pt);

	sourceTick = 62135596800000445ULL;
	destTick = 0;

	printf("62135596800000445 adding +621355968000 ticks: %d\n", sceRtcTickAddTicks(&destTick, &sourceTick, 621355968000));
	printf("source tick %llu\n", sourceTick);
	sceRtcSetTick(&pt, &sourceTick);
	DumpPSPTime("",&pt);

	printf("dest tick %llu\n", destTick);
	sceRtcSetTick(&pt, &destTick);
	DumpPSPTime("",&pt);

}

void checkAddDateValue(int year, int month, int day, int hour, int min, int sec, int micro, int type, long long int value_add)
{
	pspTime pt;
	u64 sourceTick;

	FillPSPTime(&pt,year, month, day, hour, min, sec, micro);
	sceRtcGetTick(&pt, &sourceTick);
	u64 destTick = 0;
	
	switch(type)
	{
		case 0:
			printf("%llu adding %lld years: %08x\n", sourceTick, value_add, sceRtcTickAddYears(&destTick, &sourceTick, value_add));
		break;
		case 1:
			printf("%llu adding %lld months: %08x\n", sourceTick, value_add, sceRtcTickAddMonths(&destTick, &sourceTick, value_add));
		break;
		case 2:
			printf("%llu adding %lld days: %08x\n", sourceTick, value_add, sceRtcTickAddDays(&destTick, &sourceTick, value_add));
		break;
		case 3:
			printf("%llu adding %lld hours: %08x\n", sourceTick, value_add, sceRtcTickAddHours(&destTick, &sourceTick, value_add));
		break;
		case 4:
			printf("%llu adding %lld minutes: %08x\n", sourceTick, value_add, sceRtcTickAddMinutes(&destTick, &sourceTick, value_add));
		break;
		case 5:
			printf("%llu adding %lld seconds: %08x\n", sourceTick, value_add, sceRtcTickAddSeconds(&destTick, &sourceTick, value_add));
		break;
		case 6:
			printf("%llu adding %lld microseconds: %08x\n", sourceTick, value_add, sceRtcTickAddMicroseconds(&destTick, &sourceTick, value_add));
		break;
		case 7:
			printf("%llu adding %lld weeks: %08x\n", sourceTick, value_add, sceRtcTickAddWeeks(&destTick, &sourceTick, value_add));
		break;
		default:
		break;
	}
	DumpTick("source tick", sourceTick);
	DumpTick("dest tick", destTick);
}

void checkRtcTickAddYears()
{
	printf("Checking checkRtcTickAddYears\n");

	checkAddDateValue(1970,1,1,0,0,0,445,0,-2000);
	checkAddDateValue(1970,1,1,0,0,0,445,0,2000);
	checkAddDateValue(1970,1,1,0,0,0,445,0,-20);
	checkAddDateValue(10,1,1,0,0,0,0,0,-8);		// Check limit down
	checkAddDateValue(10,1,1,0,0,0,0,0,-9);
	checkAddDateValue(10,1,1,0,0,0,0,0,-10);
	checkAddDateValue(9998,1,1,0,0,0,0,0,1);	// Check limit up
	checkAddDateValue(9998,1,1,0,0,0,0,0,2);

}

void checkIsLeapYear()
{
	printf("Checking sceRtcIsLeapYear\n");
	printf("Leap Years:\n");
	printf("1636: %08x\n", sceRtcIsLeapYear(1636));
	printf("1776: %08x\n", sceRtcIsLeapYear(1776));
	printf("1872: %08x\n", sceRtcIsLeapYear(1872));
	printf("1948: %08x\n", sceRtcIsLeapYear(1948));
	printf("2004: %08x\n", sceRtcIsLeapYear(2004));
	printf("2096: %08x\n", sceRtcIsLeapYear(2096));
	printf("Non-Leap Years:\n");
	printf("1637: %08x\n", sceRtcIsLeapYear(1637));
	printf("1777: %08x\n", sceRtcIsLeapYear(1777));
	printf("1873: %08x\n", sceRtcIsLeapYear(1873));
	printf("1949: %08x\n", sceRtcIsLeapYear(1949));
	printf("2005: %08x\n", sceRtcIsLeapYear(2005));
	printf("2097: %08x\n", sceRtcIsLeapYear(2097));
}

void checkRtcConvertLocalTimeToUTC()
{
	printf("Checking sceRtcConvertLocalTimeToUTC\n");
	u64 tick1=62135596800000000ULL;
	u64 tick2=0; 
	sceRtcConvertLocalTimeToUTC(&tick1, &tick2);
	printf("epoch : %llu\n", tick1);
	printf("epoch as UTC: %llu\n", tick2);
}

void checkRtcConvertUtcToLocalTime()
{
	printf("Checking checkRtcConvertUtcToLocalTime\n");
	u64 tick1=62135596800000000ULL;
	u64 tick2=0; 
	sceRtcConvertUtcToLocalTime(&tick1, &tick2);
	printf("epoch as UTC: %llu\n", tick1);
	printf("epoch as UTC as Local : %llu\n", tick2);
}

void checkRtcCheckValid()
{
	printf("Checking sceRtcCheckValid\n");

	pspTime pt;
	
	FillPSPTime(&pt,2012,9,20,7,0,0,0);
	printf("Valid Date:%d\n", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,-98,56,100,26,61,61,100000000);
	printf("Very invalid Date:%d\n", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,-98,9,20,7,0,0,0);
	printf("invalid year:%d\n", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,-9,20,7,0,0,0);
	printf("invalid month:%d\n", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,9,-20,7,0,0,0);
	printf("invalid day:%d\n", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,9,20,-7,0,0,0);
	printf("invalid hour:%d\n", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,9,20,7,-10,10,10);
	printf("invalid minutes:%d\n", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,9,20,7,10,-10,10);
	printf("invalid seconds:%d\n", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,9,20,7,10,10,-10);
	printf("invalid microseconds:%d\n", sceRtcCheckValid(&pt));

}

void checkMaxYear()
{
	printf("Checking sceRtcCheckValid for maximum year\n");

	int result, y;

	pspTime pt;
	FillPSPTime(&pt,1,1,1,0,0,0,1);
	for (y = 1; y < SHRT_MAX; y++) 
	{
		pt.year = y;
		result = sceRtcCheckValid(&pt);
		if (result != 0) 
		{
			printf("Max year: %d, struct year: %d, result: %d\n", y, pt.year, result);
			break;
		}
	}
}

void checkRtcSetTime_t()
{
	printf("Checking sceRtcSetTime_t\n");

	pspTime pt;
	printf("from 0:%d\n", sceRtcSetTime_t(&pt, 0));
	DumpPSPTime("", &pt);
	printf("from epoc:%d\n",sceRtcSetTime_t(&pt, 62135596800ULL));
	DumpPSPTime("", &pt);
	printf("from epoc(64):%d\n",sceRtcSetTime64_t(&pt, 62135596800000000ULL));
	DumpPSPTime("", &pt);
	printf("from 2012, 9, 20, 7, 12, 15, 500:%d\n",sceRtcSetTime_t(&pt, 1348125135));
	DumpPSPTime("", &pt);

	printf("from epoc&0xffffffff:%d\n",sceRtcSetTime_t(&pt, 62135596800ULL&0xffffffff));
	DumpPSPTime("", &pt);

}

void checkRtcGetTime_t()
{
	printf("Checking sceRtcGetTime_t\n");
	pspTime pt;
	u64 ticks=0;
	FillPSPTime(&pt,2012,9,20,7,12,15,500);
	printf("from epoc:%d\n",sceRtcGetTime_t( &pt, &ticks));
	DumpPSPTime("", &pt);
	printf("ticks: %llu\n", ticks);
	printf("from epoc (64):%d\n",sceRtcGetTime64_t( &pt, &ticks));
	DumpPSPTime("", &pt);
	printf("ticks: %llu\n", ticks);
	sceRtcSetTime_t(&pt, ticks);
	DumpPSPTime("time_t time : ",&pt);
}

void checkRtcSetDosTime()
{
	printf("Checking sceRtcSetDosTime\n");

	pspTime pt;
	printf("from epoc:%d\n",sceRtcSetDosTime(&pt, 0));
	DumpPSPTime("0 = ",&pt);
	printf("from epoc:%d\n",sceRtcSetDosTime(&pt, 1));
	DumpPSPTime("1 = ",&pt);
	printf("from epoc:%d\n",sceRtcSetDosTime(&pt, 10));
	DumpPSPTime("10 = ",&pt);
	printf("from epoc:%d\n",sceRtcSetDosTime(&pt, 100));
	DumpPSPTime("100 = ",&pt);
	printf("from epoc:%d\n",sceRtcSetDosTime(&pt, 1000));
	DumpPSPTime("1000 = ",&pt);
	printf("from epoc:%d\n",sceRtcSetDosTime(&pt, 10000));
	DumpPSPTime("10000 = ",&pt);
	printf("from epoc:%d\n",sceRtcSetDosTime(&pt, 100000));
	DumpPSPTime("100000 = ",&pt);
	printf("from epoc:%d\n",sceRtcSetDosTime(&pt, 1000000));
	DumpPSPTime("1000000 = ",&pt);
	printf("from epoc:%d\n",sceRtcSetDosTime(&pt, 10000000));
	DumpPSPTime("10000000 = ",&pt);
	printf("from epoc:%d\n",sceRtcSetDosTime(&pt, 62135596800000000ULL));
	DumpPSPTime("62135596800000000ULL = ",&pt);
}

void checkRtcGetDosTime()
{
	printf("Checking sceRtcGetDosTime\n");

	pspTime pt;
	u64 ticks=0;
	FillPSPTime(&pt, 2107, 9, 11, 24, 0, 0, 0);
	printf("from epoc:%d\n",sceRtcGetDosTime( &pt, &ticks));
	DumpPSPTime("", &pt);
	printf("ticks: %llu\n", ticks);
	
	// false date
	pt.year = 1979;
	printf("from epoc:%d\n",sceRtcGetDosTime( &pt, &ticks));
	pt.year = 2108;
	printf("from epoc:%d\n",sceRtcGetDosTime( &pt, &ticks));

}


void checkRtcCompareTick()
{
	printf("Checking sceRtcCompareTick\n");

	u64 tickbig1 = 62135596800000000ULL;
	u64 tickbig2 = 62135596800000000ULL;
	u64 ticksmall1 = 500;
	u64 ticksmall2 = 500;

	printf("big small :%d\n",sceRtcCompareTick(&tickbig1, &ticksmall1));
	printf("big big :%d\n",sceRtcCompareTick(&tickbig1, &tickbig2));
	printf("small big :%d\n",sceRtcCompareTick(&ticksmall1, &tickbig2));
	printf("small small :%d\n",sceRtcCompareTick(&ticksmall1, &ticksmall2));
}

void checkRtcTickAddMicroseconds()
{
	printf("Checking sceRtcTickAddMicroseconds\n");

	checkAddDateValue(1970,1,1,0,0,0,445,6,-2000);
	checkAddDateValue(1970,1,1,0,0,0,445,6,2000);
	checkAddDateValue(1970,1,1,0,0,0,445,6,-62135596800000445);
	checkAddDateValue(1970,1,1,0,0,0,445,6,62135596800000445);
	checkAddDateValue(1,1,1,0,0,0,10,6,-10);
	checkAddDateValue(1,1,1,0,0,0,10,6,-11);
	checkAddDateValue(9999,12,31,23,59,59,99999998,6,1);
	checkAddDateValue(9999,12,31,23,59,59,99999998,6,2);
}

void checkRtcTickAddSeconds()
{
	printf("Checking sceRtcTickAddSeconds\n");

	checkAddDateValue(1970,1,1,0,0,0,445,5,-2000);
	checkAddDateValue(1970,1,1,0,0,0,445,5,2000);
	checkAddDateValue(1970,1,1,0,0,0,445,5,-62135596800000445);
	checkAddDateValue(1970,1,1,0,0,0,445,5,62135596800000445);
	checkAddDateValue(1,1,1,0,0,10,0,5,-10);
	checkAddDateValue(1,1,1,0,0,10,0,5,-11);
	checkAddDateValue(9999,12,31,23,59,50,0,5,9);
	checkAddDateValue(9999,12,31,23,59,50,0,5,10);

	int i;
	for(i = 0; i < 70; i++)
	{
		checkAddDateValue(1970,1,1,0,0,0,445,5,-i);
		checkAddDateValue(1970,1,1,0,0,0,445,5,+i);
	}
}

void checkRtcTickAddMinutes()
{
	printf("Checking sceRtcTickAddMinutes\n");

	checkAddDateValue(1970,1,1,0,0,0,445,4,-2000);
	checkAddDateValue(1970,1,1,0,0,0,445,4,2000);
	checkAddDateValue(1970,1,1,0,0,0,445,4,-62135596800000445);
	checkAddDateValue(1970,1,1,0,0,0,445,4,62135596800000445);
	checkAddDateValue(1,1,1,0,10,0,0,4,-10);
	checkAddDateValue(1,1,1,0,10,0,0,4,-11);
	checkAddDateValue(9999,12,31,23,50,0,0,4,9);
	checkAddDateValue(9999,12,31,23,50,0,0,4,10);

	int i;
	for(i = 0; i < 70; i++)
	{
		checkAddDateValue(1970,1,1,0,0,0,445,4,-i);
		checkAddDateValue(1970,1,1,0,0,0,445,4,+i);
	}
}

void checkRtcTickAddHours()
{
	printf("Checking sceRtcTickAddHours\n");

	checkAddDateValue(1970,1,1,0,0,0,445,3,-2000);
	checkAddDateValue(1970,1,1,0,0,0,445,3,2000);
	checkAddDateValue(1970,1,1,0,0,0,445,3,-62135596800000445);
	checkAddDateValue(1970,1,1,0,0,0,445,3,62135596800000445);
	checkAddDateValue(1,1,1,10,0,0,0,3,-10);
	checkAddDateValue(1,1,1,10,0,0,0,3,-11);
	checkAddDateValue(9999,12,31,20,0,0,0,3,3);
	checkAddDateValue(9999,12,31,20,0,0,0,3,4);
	
	int i;
	for(i = 0; i < 30; i++)
	{
		checkAddDateValue(1970,1,1,0,0,0,445,3,-i);
		checkAddDateValue(1970,1,1,0,0,0,445,3,+i);
	}
}

void checkRtcTickAddDays()
{
	printf("Checking sceRtcTickAddDays\n");

	checkAddDateValue(1970,1,1,0,0,0,445,2,-2000);
	checkAddDateValue(1970,1,1,0,0,0,445,2,2000);
	checkAddDateValue(1970,1,1,0,0,0,445,2,-62135596800000445);
	checkAddDateValue(1970,1,1,0,0,0,445,2,62135596800000445);
	checkAddDateValue(1,1,10,0,0,0,0,2,-9);
	checkAddDateValue(1,1,10,0,0,0,0,2,-10);
	checkAddDateValue(9999,12,20,0,0,0,0,2,11);
	checkAddDateValue(9999,12,20,0,0,0,0,2,12);
	
	int i;
	for(i = 0; i < 35; i++)
	{
		checkAddDateValue(1970,1,1,0,0,0,445,2,-i);
		checkAddDateValue(1970,1,1,0,0,0,445,2,+i);
	}
}

void checkRtcTickAddWeeks()
{
	printf("Checking sceRtcTickAddWeeks\n");
	checkAddDateValue(1970,1,1,0,0,0,445,7,-2000);
	checkAddDateValue(1970,1,1,0,0,0,445,7,2000);
	checkAddDateValue(1970,1,1,0,0,0,445,7,-62135596800000445);
	checkAddDateValue(1970,1,1,0,0,0,445,7,62135596800000445);
	checkAddDateValue(1,1,8,0,0,0,0,7,-1);
	checkAddDateValue(1,1,8,0,0,0,0,7,-2);
	checkAddDateValue(9999,12,20,0,0,0,0,7,1);
	checkAddDateValue(9999,12,20,0,0,0,0,7,2);
	
	int i;
	for(i = 0; i < 55; i++)
	{
		checkAddDateValue(1970,1,1,0,0,0,445,7,-i);
		checkAddDateValue(1970,1,1,0,0,0,445,7,+i);
	}

}

void checkRtcTickAddMonths()
{
	printf("Checking sceRtcTickAddWeeks\n");
	checkAddDateValue(1970,1,1,0,0,0,445,1,-2000);
	checkAddDateValue(1970,1,1,0,0,0,445,1,2000);
	checkAddDateValue(1970,1,1,0,0,0,445,1,-62135596800000445);
	checkAddDateValue(1970,1,1,0,0,0,445,1,62135596800000445);
	checkAddDateValue(1,2,1,0,0,0,0,1,-1);
	checkAddDateValue(1,2,1,0,0,0,0,1,-2);
	checkAddDateValue(9999,11,1,0,0,0,0,1,1);
	checkAddDateValue(9999,11,1,0,0,0,0,1,2);

	int i;
	for(i = 0; i < 15; i++)
	{
		checkAddDateValue(1970,1,1,0,0,0,445,1,-i);
		checkAddDateValue(1970,1,1,0,0,0,445,1,+i);
	}
}

void checkRtcParseDateTime()
{
	printf("Checking sceRtcParseDateTime\n");
	printf("UNTESTED!\n");
}



void checkRtcSetWin32FileTime()
{
	pspTime pt;
	u64 ft;
	int result;
	memset(&pt, 0, sizeof(pt));

	printf("Checking sceRtcSetWin32FileTime\n");
	// Crash.
	//printf("  NULL args: %08x\n", sceRtcSetWin32FileTime(NULL, NULL));
	ft = 0;
	result = sceRtcSetWin32FileTime(&pt, ft);
	printf("  Zero time (1601 January 01): (%08x)", result);
	DumpPSPTime("", &pt);
	ft = 127779156600000010ULL;
	result = sceRtcSetWin32FileTime(&pt, ft);
	printf("  Arbitrary date/time: (%08x)", result);
	DumpPSPTime("", &pt);

	// This gives weird results.  Games unlikely to call it, so let's ignore for now.
	//ft = -(365ULL * 86400ULL * 10000000ULL);
	//result = sceRtcSetWin32FileTime(&pt, ft);
	//printf("  Before 1601 January 01: (%08x)", result);
	//DumpPSPTime("", &pt);
	printf("\n");
}

void checkRtcGetWin32FileTime()
{
	pspTime pt;
	u64 ft = -1337;
	int result;

	printf("Checking sceRtcGetWin32FileTime\n");

	// It's not clear, but it seems like sceRtcGetWin32FileTime() fails to return error messages
	// properly if it hasn't processed at least one error properly.
	// Trying to emulate this isn't necessarily a bad idea, but it's hard to determine its rules...
	FillPSPTime(&pt, 1600, 1, 1, 0, 0, 0, 0);
	sceRtcGetWin32FileTime(&pt, &ft);

	// Crash.
	//printf("  NULL args: %08x\n", sceRtcGetWin32FileTime(NULL, NULL));
	ft = -1337;
	result = sceRtcGetWin32FileTime(&pt, NULL);
	printf("  NULL filetime: %lld (%08x)\n", ft, result);
	memset(&pt, 0, sizeof(pt));
	ft = -1337;
	result = sceRtcGetWin32FileTime(&pt, &ft);
	printf("  Zeroed time: %lld (%08x)\n", ft, result);
	FillPSPTime(&pt, 2005, 11, 31, 13, 01, 00, 1);
	ft = -1337;
	result = sceRtcGetWin32FileTime(&pt, &ft);
	printf("  Arbitrary date/time: %lld (%08x)\n", ft, result);
	FillPSPTime(&pt, 1601, 1, 1, 0, 0, 0, 0);
	ft = -1337;
	result = sceRtcGetWin32FileTime(&pt, &ft);
	printf("  1601 January 01: %lld (%08x)\n", ft, result);
	FillPSPTime(&pt, 1600, 1, 1, 0, 0, 0, 0);
	ft = -1337;
	result = sceRtcGetWin32FileTime(&pt, &ft);
	printf("  1600 January 01: %lld (%08x)\n", ft, result);
	FillPSPTime(&pt, 0, 1, 1, 0, 0, 0, 0);
	ft = -1337;
	result = sceRtcGetWin32FileTime(&pt, &ft);
	printf("  1 January 01: %lld (%08x)\n", ft, result);
	printf("\n");
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
	checkIsLeapYear();
	checkRtcConvertLocalTimeToUTC();
	checkRtcConvertUtcToLocalTime();
	checkRtcCheckValid();

	checkMaxYear();
	checkRtcSetTime_t();
	checkRtcGetTime_t();
	checkRtcSetDosTime();
	checkRtcGetDosTime();
	checkRtcSetWin32FileTime();
	checkRtcGetWin32FileTime();
	checkRtcCompareTick();
	checkRtcTickAddMicroseconds();
	checkRtcTickAddSeconds();
	checkRtcTickAddMinutes();
	checkRtcTickAddHours();
	checkRtcTickAddWeeks();
	checkRtcTickAddMonths();
	checkRtcParseDateTime();

	return 0;
}

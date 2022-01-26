#include <common.h>
#include "rtc_common.h"

void checkDaysInMonth() {
	checkpointNext("Checking sceRtcGetDaysInMonth");
	checkpoint("sceRtcGetDaysInMonth:2010, 4: %d", sceRtcGetDaysInMonth(2010, 4));
}

static void testRtcGetDayOfWeek(int y, int m, int d) {
	checkpoint("sceRtcGetDayOfWeek:%d, %d, %d: %d", y, m, d, sceRtcGetDayOfWeek(y, m, d));
}

void checkDayOfWeek() {
	checkpointNext("Checking sceRtcGetDayOfWeek");
	testRtcGetDayOfWeek(2010, 4, 27);
	
	//A game does this: sceRtcGetDayOfWeek(166970016, 1024, 0)
	testRtcGetDayOfWeek(166970016, 1024, 0);
	
	// test random no valid date
	// leap year
	testRtcGetDayOfWeek(2000, 0, 0);
	testRtcGetDayOfWeek(2000, 1, 0);
	testRtcGetDayOfWeek(2000, 573, 0);
	testRtcGetDayOfWeek(2000, 1, 2458);
	testRtcGetDayOfWeek(2000, 4587, 2458);
	
	// standard year
	testRtcGetDayOfWeek(2001, 0, 0);
	testRtcGetDayOfWeek(2001, 1, 0);
	testRtcGetDayOfWeek(2001, 573, 0);
	testRtcGetDayOfWeek(2001, 1, 2458);
	testRtcGetDayOfWeek(2001, 4587, 2458);
}

void checkIsLeapYear() {
	checkpointNext("Checking sceRtcIsLeapYear");
	checkpointNext("Leap Years:");
	checkpoint("1636: %08x", sceRtcIsLeapYear(1636));
	checkpoint("1776: %08x", sceRtcIsLeapYear(1776));
	checkpoint("1872: %08x", sceRtcIsLeapYear(1872));
	checkpoint("1948: %08x", sceRtcIsLeapYear(1948));
	checkpoint("2004: %08x", sceRtcIsLeapYear(2004));
	checkpoint("2096: %08x", sceRtcIsLeapYear(2096));
	checkpointNext("Non-Leap Years:");
	checkpoint("1637: %08x", sceRtcIsLeapYear(1637));
	checkpoint("1777: %08x", sceRtcIsLeapYear(1777));
	checkpoint("1873: %08x", sceRtcIsLeapYear(1873));
	checkpoint("1949: %08x", sceRtcIsLeapYear(1949));
	checkpoint("2005: %08x", sceRtcIsLeapYear(2005));
	checkpoint("2097: %08x", sceRtcIsLeapYear(2097));
}

void checkMaxYear() {
	checkpointNext("Checking sceRtcCheckValid for maximum year");

	int result, y;

	pspTime pt;
	FillPSPTime(&pt,1,1,1,0,0,0,1);
	for (y = 1; y < SHRT_MAX; y++) {
		pt.year = y;
		result = sceRtcCheckValid(&pt);
		if (result != 0) {
			checkpoint("Max year: %d, struct year: %d, result: %d", y, pt.year, result);
			break;
		}
	}
}

void checkRtcCheckValid() {
	checkpointNext("Checking sceRtcCheckValid");

	pspTime pt;
	
	FillPSPTime(&pt,2012,9,20,7,0,0,0);
	checkpoint("Valid Date: %d", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,-98,56,100,26,61,61,100000000);
	checkpoint("Very invalid Date: %d", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,-98,9,20,7,0,0,0);
	checkpoint("invalid year: %d", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,-9,20,7,0,0,0);
	checkpoint("invalid month: %d", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,9,-20,7,0,0,0);
	checkpoint("invalid day: %d", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,9,20,-7,0,0,0);
	checkpoint("invalid hour: %d", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,9,20,7,-10,10,10);
	checkpoint("invalid minutes: %d", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,9,20,7,10,-10,10);
	checkpoint("invalid seconds: %d", sceRtcCheckValid(&pt));

	FillPSPTime(&pt,2012,9,20,7,10,10,-10);
	checkpoint("invalid microseconds: %d", sceRtcCheckValid(&pt));
}

int main(int argc, char **argv) {
	checkDaysInMonth();
	checkDayOfWeek();
	checkIsLeapYear();
	checkMaxYear();
	checkRtcCheckValid();

	return 0;
}
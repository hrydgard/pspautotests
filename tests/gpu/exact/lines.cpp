#include "jobs.h"

// Lines: diamond exit with endpoint ties, Gouraud color along lines, antialiased line alpha, and lines
// shorter than a pixel.
// Each job prints a CRC of its readback; the job names give the ppsspp-re geprobe experiment to rerun.

#include "lines_jobs.h"

extern "C" int main(int argc, char *argv[]) {
	return runJobs((const u8 *)jobData, jobs, sizeof(jobs) / sizeof(jobs[0]));
}

#include "jobs.h"

// Which pixels a triangle covers: zero-area triangles, huge triangles and the 4-pixel span snapping of very
// tall edges, bilinear sprites starting left of x = 0, and a rectangle drawn as two triangles.
// Each job prints a CRC of its readback; the job names give the ppsspp-re geprobe experiment to rerun.

#include "coverage_jobs.h"

extern "C" int main(int argc, char *argv[]) {
	return runJobs((const u8 *)jobData, jobs, sizeof(jobs) / sizeof(jobs[0]));
}

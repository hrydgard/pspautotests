#include "jobs.h"

// Depth and Gouraud color planes: the triangle setup reciprocal, 14-bit gradients, the anchor vertex, and
// colors on random triangles of every size.
// Each job prints a CRC of its readback; the job names give the ppsspp-re geprobe experiment to rerun.

#include "planes_jobs.h"

extern "C" int main(int argc, char *argv[]) {
	return runJobs((const u8 *)jobData, jobs, sizeof(jobs) / sizeof(jobs[0]));
}

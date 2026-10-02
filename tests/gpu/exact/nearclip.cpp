#include "jobs.h"

// Near-plane clipping: one and two vertices behind the plane, vertex order in strips and fans, and dense
// small triangles with heavy cancellation.
// Each job prints a CRC of its readback; the job names give the ppsspp-re geprobe experiment to rerun.

#include "nearclip_jobs.h"

extern "C" int main(int argc, char *argv[]) {
	return runJobs((const u8 *)jobData, jobs, sizeof(jobs) / sizeof(jobs[0]));
}

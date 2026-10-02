#include "jobs.h"

// The vertex transform and what comes before rasterizing: where points land in x/y, fog, depth beyond the far
// plane and the viewport, the depth clamp, vertices behind the camera, culling, and the per-pixel depth
// floor.
// Each job prints a CRC of its readback; the job names give the ppsspp-re geprobe experiment to rerun.

#include "transform_jobs.h"

extern "C" int main(int argc, char *argv[]) {
	return runJobs((const u8 *)jobData, jobs, sizeof(jobs) / sizeof(jobs[0]));
}

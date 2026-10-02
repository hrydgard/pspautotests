#include "jobs.h"

// Lighting arithmetic: a spot factor from a game, and the vertex-to-light vector as a matrix row, read
// through point and spot lights.
// Each job prints a CRC of its readback; the job names give the ppsspp-re geprobe experiment to rerun.

#include "lighting_jobs.h"

extern "C" int main(int argc, char *argv[]) {
	return runJobs((const u8 *)jobData, jobs, sizeof(jobs) / sizeof(jobs[0]));
}

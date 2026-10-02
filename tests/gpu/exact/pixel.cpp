#include "jobs.h"

// Per-pixel details: zero-alpha texels with dither, drawing past the buffer stride, small scaled sprites,
// through-mode fog, the texture cache across draws, and the alpha test on filtered alpha.
// Each job prints a CRC of its readback; the job names give the ppsspp-re geprobe experiment to rerun.

#include "pixel_jobs.h"

extern "C" int main(int argc, char *argv[]) {
	return runJobs((const u8 *)jobData, jobs, sizeof(jobs) / sizeof(jobs[0]));
}

#include "jobs.h"

// Texturing: texture functions and bilinear weights, dither and 565, mip level selection in slope and auto
// modes, CLUTs per mip level, 16-bit texels, fragment alpha, and texture decoding and addressing.
// Each job prints a CRC of its readback; the job names give the ppsspp-re geprobe experiment to rerun.

#include "texture_jobs.h"

extern "C" int main(int argc, char *argv[]) {
	return runJobs((const u8 *)jobData, jobs, sizeof(jobs) / sizeof(jobs[0]));
}

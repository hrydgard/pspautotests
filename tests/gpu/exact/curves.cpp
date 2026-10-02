#include "jobs.h"

// Bezier and spline tessellation: fixed-point de Casteljau and de Boor, mixed signs and exponents, normals
// from the tangents, and patches drawn as lines.
// Each job prints a CRC of its readback; the job names give the ppsspp-re geprobe experiment to rerun.

#include "curves_jobs.h"

extern "C" int main(int argc, char *argv[]) {
	return runJobs((const u8 *)jobData, jobs, sizeof(jobs) / sizeof(jobs[0]));
}

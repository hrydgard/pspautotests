#pragma once

#include "shared.h"

// A test here is a set of GE probe jobs (ppsspp-re tools/geprobe), embedded in the probe's job format
// by gentests.py there. Each job clears the buffers, runs a display list built on the host and reads a
// buffer back; the test prints a CRC of each readback, so any difference from the PSP shows, by job.
struct ExactJob {
	const char *name;
};

int runJobs(const u8 *data, const ExactJob *jobs, int count);

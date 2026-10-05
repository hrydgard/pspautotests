#pragma once

typedef struct {
	int called;
	int stackSize;
	unsigned int attr;
	int priority;
	unsigned int stack;
} ThreadRecord;

typedef struct {
	ThreadRecord start;
	ThreadRecord stop;
} Results;

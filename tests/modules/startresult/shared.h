// What the test passes to the child's module_start and module_stop: the value each should return.
typedef struct {
	int startReturn;
	int stopReturn;
	int startCalls;
	int stopCalls;
} Control;

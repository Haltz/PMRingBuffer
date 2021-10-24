#include "./RingBuffer.h"

static int testv1(void);

int main() {
	printf("No test is configured.\n");
}

static int testv1(void) {
	RingBufferQueue *q = NULL;
	RingBufferQueueOps qops = (RingBufferQueueOps){
		.init = queue_init, .exit = queue_exit, .pop = queue_pop, .push = queue_push, .top = queue_top, .empty = queue_empty, .print = NULL};

	if (queue_init(&q) != RingBufferInit_OK) {
		printf("Queue Init Err.\n");
		return 0;
	}

	int *iter, dat[10];
	char log_pr[255];
	for (int i = 0; i < 10; i++) {
		dat[i] = rand() % 1000000;
		qops.push(q, &dat[i]);
	}

	while (q->size) {
		qops.pop(q, (void **)&iter);
		printf("%d, ", *iter);
	}

	qops.exit(&q);
}

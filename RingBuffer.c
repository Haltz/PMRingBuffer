/**
 * @file RingBuffer.c
 * @author Haltz (mudong.huang@gmail.com)
 * @brief This part of code is for managing a ring buffer (one producer, one consumer)
 * All operations is single-threaded. I use a FIFO queue to protect this feature.
 * It also means queue can only be operated by only one thread, I called it controller.
 * @version 0.1
 * @date 2021-10-23
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "./RingBuffer.h"

#define KB (1024)
#define MB (1024 * 1024)

#define DEFAULT_RINGBUFFER_POOL_SIZE (4 * KB)

#define min(x, y) ((x) < (y) ? (x) : (y))

static inline void inc_ring_buffer_head(RingBuffer *ring, int size) {
	pthread_spin_lock(&ring->meta_lock);
	ring->head += size;
	pthread_spin_unlock(&ring->meta_lock);
}

static inline int get_ring_buffer_head(RingBuffer *ring) {
	pthread_spin_lock(&ring->meta_lock);
	int r = ring->total_size;
	pthread_spin_unlock(&ring->meta_lock);

	return r;
}

static inline void inc_ring_buffer_tail(RingBuffer *ring, int size) {
	pthread_spin_lock(&ring->meta_lock);
	ring->tail += size;
	pthread_spin_unlock(&ring->meta_lock);
}

static inline int get_ring_buffer_tail(RingBuffer *ring) {
	pthread_spin_lock(&ring->meta_lock);
	int r = ring->tail;
	pthread_spin_unlock(&ring->meta_lock);

	return r;
}

static inline void inc_ring_buffer_size(RingBuffer *ring, int size) {
	pthread_spin_lock(&ring->meta_lock);
	ring->total_size += size;
	pthread_spin_unlock(&ring->meta_lock);
}

static inline int get_ring_buffer_size(RingBuffer *ring) {
	pthread_spin_lock(&ring->meta_lock);
	int r = ring->total_size;
	pthread_spin_unlock(&ring->meta_lock);

	return r;
}

static inline int get_ring_buffer_queue_size(RingBufferQueue *q) {
	pthread_spin_lock(&q->sz_lock);
	int res = q->size;
	pthread_spin_unlock(&q->sz_lock);
	return res;
}

static inline void inc_ring_buffer_queue_size(RingBufferQueue *q, int nr) {
	pthread_spin_lock(&q->sz_lock);
	q->size += nr;
	pthread_spin_unlock(&q->sz_lock);
}

RingBufferIOTask *make_io_task(RingBufferIOTaskType type, int offset, int size) {
	RingBufferIOTask *io_task = (RingBufferIOTask *)malloc(sizeof(RingBufferIOTask));
	if (!io_task) {
		return NULL;
	}

	io_task->offset = offset;
	io_task->size = size;
	io_task->type = type;

	return io_task;
}

void destroy_io_task(RingBufferIOTask *io_task) {
	if (io_task->state == ONGOING) {
		printf("Can't cancel an io task under process.\n");
		return;
	}

	free(io_task);
}

RingBufferQueueOpRes queue_init(RingBufferQueue **q) {
	*q = NULL;

	*q = malloc(sizeof(RingBufferQueue));
	if (!*q) {
		return RingBufferQueueOp_NOMEM;
	}

	(*q)->head = (*q)->tail = NULL;
	(*q)->size = 0;

	pthread_spin_init(&(*q)->sz_lock, PTHREAD_PROCESS_SHARED);

	printf("Init queue.\n");

	return RingBufferQueueOp_OK;
}

RingBufferQueueOpRes queue_exit(RingBufferQueue **q) {
	if (!*q) {
		return RingBufferQueueOp_OK;
	}

	while ((*q)->head) {
		RingBufferQueueNode *to_free = (*q)->head;
		(*q)->head = (*q)->head->next;
		free(to_free);
	}

	pthread_spin_destroy(&(*q)->sz_lock);

	free(*q);

	return RingBufferQueueOp_OK;
}

RingBufferQueueOpRes queue_push(RingBufferQueue *q, void *data) {
	if (!q) {
		return RingBufferQueueOp_NULL;
	}

	RingBufferQueueNode *new_node = (RingBufferQueueNode *)malloc(sizeof(RingBufferQueueNode));
	if (!new_node) {
		printf("Malloc fault.\n");
		return RingBufferQueueOp_NOMEM;
	}

	new_node->next = NULL;
	new_node->node = data;

	if (q->head) {
		q->tail->next = new_node;
		new_node->prev = q->tail;

		q->tail = new_node;
	} else {
		q->head = q->tail = new_node;
		new_node->prev = NULL;
	}

	q->size++;
	return RingBufferQueueOp_OK;
}

RingBufferQueueOpRes queue_pop(RingBufferQueue *q, void **data) {
	if (!q || !q->size) {
		return RingBufferQueueOp_NULL;
	}

	if (q->head == q->tail) {
		*data = q->head->node;
		free(q->head);
		q->head = q->tail = NULL;
	} else {
		*data = q->head->node;
		free(q->head);
		q->head = q->head->next;
	}

	q->size--;
	return RingBufferQueueOp_OK;
}

RingBufferQueueOpRes queue_top(RingBufferQueue *q, void **data) {
	if (!q || !q->size) {
		return RingBufferQueueOp_NULL;
	}

	*data = q->head->node;
	return RingBufferQueueOp_OK;
}

RingBufferQueueOpRes queue_print(RingBufferQueue *q) {
	printf("Print func is not supported now.\n");
	return RingBufferInit_ERR;
}

bool queue_empty(RingBufferQueue *q) {
	pthread_spin_lock(&q->sz_lock);
	bool is_empty = (bool)q->size;
	pthread_spin_unlock(&q->sz_lock);

	return is_empty;
}

static int ring_buffer_write(RingBufferController *ctrl, int size, void *data) {
	// TODO: Add a write task on the ring buffer write queue.
	RingBufferIOTask *iotask = make_io_task(RingBufferIOTask_WRITE, get_ring_buffer_size(ctrl->ring), size);
	if (!iotask) {
		return 1;
	}

	if (ctrl->qops.push(ctrl->wr_queue, iotask) != RingBufferQueueOp_OK) {
		destroy_io_task(iotask);
		return 1;
	}

	return 0;
}

static int ring_buffer_read(RingBufferController *ctrl, int size, void *data) {
	// TODO:
	RingBufferIOTask *iotask = make_io_task(RingBufferIOTask_READ, get_ring_buffer_size(ctrl->ring), size);
	if (!iotask) {
		return 1;
	}

	if (ctrl->qops.push(ctrl->rd_queue, iotask) != RingBufferQueueOp_OK) {
		destroy_io_task(iotask);
		return 1;
	}

	return 0;
}

static int ring_buffer_flush(RingBufferController *ctrl, int size, void *data) {
	// TODO:
	RingBufferIOTask *iotask = make_io_task(RingBufferIOTask_FLUSH, get_ring_buffer_size(ctrl->ring), size);
	if (!iotask) {
		return 1;
	}

	if (ctrl->qops.push(ctrl->fl_queue, iotask) != RingBufferQueueOp_OK) {
		destroy_io_task(iotask);
		return 1;
	}

	return 0;
}

RingBufferInitState init_ringbuffer(RingBufferController **ctrl_p) {
	RingBufferController *ctrl = *ctrl_p = NULL;

	ctrl = (RingBufferController *)malloc(sizeof(RingBufferController));
	if (!ctrl) {
		return RingBufferInit_NOMEM;
	}

	ctrl->ring = (RingBuffer *)malloc(sizeof(RingBuffer));
	if (!ctrl->ring) {
		free(ctrl);
		return RingBufferInit_NOMEM;
	}

	ctrl->ring->head = ctrl->ring->tail = 0;
	ctrl->ring->total_size = DEFAULT_RINGBUFFER_POOL_SIZE;
	ctrl->ring->pool = malloc(DEFAULT_RINGBUFFER_POOL_SIZE);
	memcpy(ctrl->ring->name, "TestRingBuffer", strlen("TestRingBuffer"));

	ctrl->qops = (RingBufferQueueOps){.init = queue_init, .exit = queue_exit, .pop = queue_pop, .push = queue_push, .top = queue_top, .empty = queue_empty, .print = NULL};

	ctrl->qops.init(&ctrl->wr_queue);
	ctrl->qops.init(&ctrl->rd_queue);
	ctrl->qops.init(&ctrl->fl_queue);

	pthread_spin_init(&ctrl->ring->meta_lock, PTHREAD_PROCESS_SHARED);

	*ctrl_p = ctrl;

	return RingBufferInit_OK;
}

void exit_ringbuffer(RingBufferController **ctrl_p) {
	RingBufferController *ctrl = *ctrl_p;
	if (!ctrl) {
		return;
	}

	if (ctrl->ring) {
		if (ctrl->ring->pool) {
			free(ctrl->ring->pool);
		}
		free(ctrl->ring);
	}

	ctrl->qops.exit(&ctrl->wr_queue);
	ctrl->qops.exit(&ctrl->rd_queue);
	ctrl->qops.exit(&ctrl->fl_queue);

	pthread_spin_destroy(&ctrl->ring->meta_lock);

	free(ctrl);
	*ctrl_p = NULL;
}

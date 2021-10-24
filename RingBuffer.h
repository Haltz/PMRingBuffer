/**
 * @file RingBuffer.h
 * @author Haltz (mudong.huang@gmail.com)
 * @brief This header defines some States, some OperationInterface, and some important struct types.
 * TODO: Maybe a detailed description/comments is needed in the future.
 * @version 0.1
 * @date 2021-10-23
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef __RING_BUFFER_H
#define __RING_BUFFER_H

#include "malloc.h"
#include "math.h"
#include "pthread.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

typedef enum RingBufferInitState {
	RingBufferInit_OK = 0,
	RingBufferInit_ERR = 1,
	RingBufferInit_NOMEM = 2
} RingBufferInitState;

typedef enum RingBufferControllerOpRes {
	RingBufferControllerOp_OK = 0,
	RingBufferControllerOp_ERR = 1,
	RingBufferControllerOp_NOMEM = 2,
	RingBufferControllerOp_NULL = 3
} RingBufferControllerOpRes;

typedef enum RingBufferQueueOpRes {
	RingBufferQueueOp_OK = 0,
	RingBufferQueueOp_ERR = 1,
	RingBufferQueueOp_NOMEM = 2,
	RingBufferQueueOp_NULL = 3
} RingBufferQueueOpRes;

typedef enum RingBufferIOTaskType {
	RingBufferIOTask_WRITE = 0,
	RingBufferIOTask_READ = 1,
	RingBufferIOTask_FLUSH = 2
} RingBufferIOTaskType;

typedef enum RingBufferIOTaskState {
	READY = 0,
	ONGOING = 1,
	SUCCESS = 2,
	FAILED = 3
} RingBufferIOTaskState;

typedef struct RingBufferQueueNode {
	void *node;
	void *prev, *next;
} RingBufferQueueNode;

typedef struct RingBufferQueue {
	int size;

	pthread_spinlock_t sz_lock;

	RingBufferQueueNode *head, *tail;
} RingBufferQueue;

typedef struct RingBufferQueueOps {
	RingBufferQueueOpRes (*init)(RingBufferQueue **q);
	RingBufferQueueOpRes (*exit)(RingBufferQueue **q);
	RingBufferQueueOpRes (*push)(RingBufferQueue *q, void *data);
	RingBufferQueueOpRes (*pop)(RingBufferQueue *q, void **data);
	RingBufferQueueOpRes (*top)(RingBufferQueue *q, void **data);
	RingBufferQueueOpRes (*print)(RingBufferQueue *q);
	bool (*empty)(RingBufferQueue *q);
} RingBufferQueueOps;

typedef struct RingBufferIOTask {
	RingBufferIOTaskType type;
	RingBufferIOTaskState state;
	int offset, size;
	void *data;

	/** FIXME: I use `void *` here because compiler does not allow using `RingBufferIOTask *` directly.
	/* I believe there is a better solution.
	* */
	void *private;
	void (*excute)(void *task);
	void (*callback)(void *task);
} RingBufferIOTask;

typedef struct RingBuffer {
	char name[32];

	int total_size;
	int head, tail;

	// There is no PM on the host so use DRAM as an alternative.
	void *pool;

	pthread_spinlock_t meta_lock;
} RingBuffer;

typedef struct RingBufferControllerOps {
	/** FIXME: I use `void *` here because compiler does not allow using `RingBufferController *` directly.
	/* I believe there is a better solution.
	* */
	int (*write)(void *ring, int size, void *data);
	int (*read)(void *ring, int size, void *data);
	int (*flush)(void *ring, int size, void *data);
	int (*print)(void *ring);
} RingBufferControllerOps;

typedef struct RingBufferController {
	RingBuffer *ring;

	// Write Tasks Queue.
	RingBufferQueue *wr_queue;
	// Read Tasks Queue.
	RingBufferQueue *rd_queue;
	// Flush Tasks Queue.
	RingBufferQueue *fl_queue;

	RingBufferQueueOps qops;
	RingBufferControllerOps ioops;
} RingBufferController;

// default queue operations, without support for concurrent accesses.
RingBufferQueueOpRes queue_init(RingBufferQueue **q);
RingBufferQueueOpRes queue_exit(RingBufferQueue **q);
RingBufferQueueOpRes queue_push(RingBufferQueue *q, void *data);
RingBufferQueueOpRes queue_top(RingBufferQueue *q, void **data);
RingBufferQueueOpRes queue_pop(RingBufferQueue *q, void **data);

// default queue operations, support concurrent accesses.
bool queue_empty(RingBufferQueue *q);

// default ring buffer io operations, without support for concurrent accesses.
RingBufferControllerOpRes ringbuffer_write(RingBufferController *ctrl, int size, void *data);
RingBufferControllerOpRes ringbuffer_read(RingBufferController *ctrl, int size, void *data);
RingBufferControllerOpRes ringbuffer_flush(RingBufferController *ctrl, int size, void *data);

// helper functions.
RingBufferIOTask *make_io_task(RingBufferIOTaskType type, int offset, int size);
void destroy_io_task(RingBufferIOTask *io_task);

// Init a ring buffer, return the result.
RingBufferInitState init_ringbuffer(RingBufferController **ctrl_p);
void exit_ringbuffer(RingBufferController **ctrl_p);

#endif
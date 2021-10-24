#ifndef __PMCCONTROLLER_H
#define __PMCCONTROLLER_H

#include "RingBuffer.h"
#include "stdio.h"

typedef struct PMController {
	int file_nr;

	// Translate file id to the corresponding ring buffer controller pointer and file name.
	RingBufferController *id2rbctrl;
	char *id2filename;

	void (*run)(PMController *ctrl);
} PMController;

int init_pm_controller(PMController **ctrl);
void exit_pm_controller(PMController **ctrl);

#endif
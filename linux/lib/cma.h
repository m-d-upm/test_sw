/*
 * CMAlib - Contiguous Memory Allocator user space library
 *
 * Copyright (C) 2025 Milos Dordevic, CEI-UPM.
 * 
 */

#ifndef _CMA_H_
#define _CMA_H_

#include <stdint.h>

void *cma_alloc(uint32_t size, const char* dev_name); // returns buffer ID
void cma_free(void *ptr);

int cma_get_buff_id(void* ptr);

#endif

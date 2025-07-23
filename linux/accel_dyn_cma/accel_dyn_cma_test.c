// Copyright 2025 CEI - UPM.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Milos Dordevic <milos.dordevic@upm.es>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include <sys/mman.h>  // mmap()

#include "accel_dyn_cma.h" 

void accel_dyn_cma_test()
{
    int file_desc = open(ACCEL_DYN_CMA_DEV_NAME, O_RDWR);

    if (file_desc < 0) {
        printf("Can't open device file: %s, error:%d\n", ACCEL_DYN_CMA_DEV_NAME, file_desc);
        exit(EXIT_FAILURE);
    }

    void* buf = NULL;

    struct accel_dyn_cma_alloc_req_ioctl_arg req = { .size = 4096,
                                                     .id = -1};

    if(ioctl(file_desc, ACCEL_DYN_CMA_IOCTL_ALLOC, &req) != 0)
    {
        printf("ERROR: Failed to allocate buffer!\n");
        goto error;
    }

    printf("Allocated buffer with size: %d bytes and ID: %d\n", req.size, req.id);

    if(ioctl(file_desc, ACCEL_DYN_CMA_IOCTL_FREE, &req.id) != 0)
    {
        printf("ERROR: Failed to deallocate buffer!\n");
        goto error;
    }

    close(file_desc);

    return;

error:
    close(file_desc);

    exit(EXIT_FAILURE);
}

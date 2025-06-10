// Copyright 2025 CEI - UPM.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Juan Granja <juan.granja@upm.es>
// Milos Dordevic <milos.dordevic@upm.es>

#include "utilities.h"

#include <stdio.h>

//#define _POSIX_C_SOURCE 199309L
#include <time.h>

// https://man7.org/linux/man-pages/man2/clock_gettime.2.html
// https://stackoverflow.com/questions/7506952/understanding-the-different-clocks-of-clock-gettime

// #define CLOCK_GETTIME_CLOCK_ID CLOCK_MONOTONIC_RAW
#define CLOCK_GETTIME_CLOCK_ID CLOCK_PROCESS_CPUTIME_ID

void examine_mem(uint32_t *ptr, uint32_t offset, uint32_t size)
{
    for(int i = 0; i < size; i += 2)
    {
        printf("%08x: %08x %08x\n", offset + i * 4, *(ptr + offset + i), *(ptr + offset + i + 1));
        if((i / 8 + 1) % 4 == 0)
            printf("\n");
    }
}

// https://stackoverflow.com/questions/5833094/get-a-timestamp-in-c-in-microseconds

/// Convert seconds to milliseconds
#define SEC_TO_MS(sec) ((sec) * 1000)
/// Convert seconds to microseconds
#define SEC_TO_US(sec) ((sec) * 1000000)
/// Convert seconds to nanoseconds
#define SEC_TO_NS(sec) ((sec) * 1000000000)

/// Convert nanoseconds to seconds
#define NS_TO_SEC(ns)   ((ns) / 1000000000)
/// Convert nanoseconds to milliseconds
#define NS_TO_MS(ns)    ((ns) / 1000000)
/// Convert nanoseconds to microseconds
#define NS_TO_US(ns)    ((ns) / 1000)

/// Get a time stamp in milliseconds.
uint64_t millis()
{
    struct timespec ts;
    clock_gettime(CLOCK_GETTIME_CLOCK_ID, &ts);
    uint64_t ms = SEC_TO_MS((uint64_t)ts.tv_sec) + NS_TO_MS((uint64_t)ts.tv_nsec);
    return ms;
}

/// Get a time stamp in microseconds.
uint64_t micros()
{
    struct timespec ts;
    clock_gettime(CLOCK_GETTIME_CLOCK_ID, &ts);
    uint64_t us = SEC_TO_US((uint64_t)ts.tv_sec) + NS_TO_US((uint64_t)ts.tv_nsec);
    return us;
}

/// Get a time stamp in nanoseconds.
uint64_t nanos()
{
    struct timespec ts;
    clock_gettime(CLOCK_GETTIME_CLOCK_ID, &ts);
    uint64_t ns = SEC_TO_NS((uint64_t)ts.tv_sec) + (uint64_t)ts.tv_nsec;
    return ns;
}

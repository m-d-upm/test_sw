#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdint.h>

void examine_mem(uint32_t *ptr, uint32_t offset, uint32_t size);

uint64_t millis();
uint64_t micros();
uint64_t nanos();

#endif

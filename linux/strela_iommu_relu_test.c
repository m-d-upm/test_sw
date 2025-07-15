// Copyright 2025 CEI - UPM.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Juan Granja <juan.granja@upm.es>
// Milos Dordevic <milos.dordevic@upm.es>

#include "strela.h" 
 
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include <sys/mman.h>  // mmap()
#include <stdint.h>

#include <time.h>

#include "utilities.h"

#define CONFIG_OFFSET 0
#define INPUT_OFFSET    (0x10000)
#define OUTPUT_OFFSET   (0x20000)

#define US_TO_CYCLES(t) (unsigned int)((t) * 1e-6 * 50e6)

//#define TRANSFER_SIZE (8192) // 32 KB
 #define TRANSFER_SIZE (4096) // 16 KB

int32_t input_data[TRANSFER_SIZE];
int32_t output_data[TRANSFER_SIZE];
int32_t output_data_sw[TRANSFER_SIZE];

#define RELU_KRNL_NPE (14)
#define RELU_KRNL_SIZE (RELU_KRNL_NPE * 5)
#define RELU_KRNL_BYTES (RELU_KRNL_SIZE * 4)

static uint32_t *mmap_ptr = NULL;

uint32_t relu_kernel[RELU_KRNL_SIZE] = {
    0xC0000021, 0x00260200, 0x00000000, 0x00000000, 0xC5000000, // 0
    0xC0000024, 0x00260200, 0x00000000, 0x00000000, 0xC5010000, // 1
    0xC0810000, 0x00202130, 0x00000000, 0x00000000, 0xF1020000, // 2
    0xC0000024, 0x00260200, 0x00000000, 0x00000000, 0xC5030000, // 3

    0xC0800010, 0x00202030, 0x00000000, 0x00000000, 0xE5040000, // 4
    0x00000004, 0x00000000, 0x00000000, 0x00000000, 0x05050000, // 5
    0x02100002, 0x00000000, 0x00000000, 0x00000000, 0x25060000, // 6
    0xC0000420, 0x00202080, 0x00000000, 0x00000000, 0xCD070000, // 7

    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x05080000, // 8
    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x050A0000, // 10
    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x050B0000, // 11

    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x050C0000, // 12
    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x050E0000, // 14
    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x050F0000  // 15
};

void relu_test()
{
    int file_desc = open(DEVICE_PATH, O_RDWR);

    if (file_desc < 0) {
        printf("Can't open device file: %s, error:%d\n", DEVICE_PATH, file_desc);
        exit(EXIT_FAILURE);
    }

    printf("\n---------\n");

    // void *mmap(void addr[.length], size_t length, int prot, int flags, int fd, off_t offset);
    // NULL: Kernel chooses address
    mmap_ptr = mmap(NULL, STRELA_DATA_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, file_desc, 0);

    if (mmap_ptr == MAP_FAILED)
    {
        printf("MMAP FAILED\n");
        close(file_desc);
        exit(EXIT_FAILURE);
    }

    printf("USER: VIRTUAL ADDR: %p \n", mmap_ptr);

    // Populate input data
    for(int i = 0; i < TRANSFER_SIZE; i++)
    {
        input_data[i] = i % 2 ? i : -i;
    }

    // Read input data befor write (test cache flushing)
    printf("OUTPUT Before:\n");

    for(int i = 0; i < 20; i++)
    {
        *(mmap_ptr + OUTPUT_OFFSET / 4 + i) = 0xffffffff;
    }

    examine_mem(mmap_ptr, OUTPUT_OFFSET / 4, 20);

    // Copy input data to buffer
    uint64_t begin_write_input = micros();

    memcpy((mmap_ptr + INPUT_OFFSET), input_data, TRANSFER_SIZE * sizeof(uint32_t));

    uint64_t end_write_input = micros();

    // Copy config to buffer
    uint32_t *cgra_kernel = relu_kernel;
    uint32_t cgra_kernel_size_words = RELU_KRNL_SIZE;

    printf("Copying config...\n");

    uint64_t begin_write_config = micros();

    memcpy((mmap_ptr + CONFIG_OFFSET), cgra_kernel, cgra_kernel_size_words * sizeof(uint32_t));

    uint64_t end_write_config = micros();

    printf("Setting up transfer...\n");

    uint64_t begin_setup_transf = micros();

    struct strela_csrs cgra_ctrl = {0};

    cgra_ctrl.conf_offs = CONFIG_OFFSET;
    cgra_ctrl.conf_count = cgra_kernel_size_words;

    cgra_ctrl.in0_offs = INPUT_OFFSET;
    cgra_ctrl.in0_count = TRANSFER_SIZE;
    cgra_ctrl.in0_stride = 4;

    cgra_ctrl.out0_offs = OUTPUT_OFFSET;
    cgra_ctrl.out0_count = TRANSFER_SIZE;

    if (ioctl(file_desc, IOCTL_STRELA_CONTROL, &cgra_ctrl) != 0)
    {
        printf("ERROR: Setting up transfer!\n");
        goto error;
    }

    uint64_t end_setup_transf = micros();

    // Configure 1

    printf("Transfering config 1 to the device...\n");

    uint64_t begin_cgra_config = micros();

    if (ioctl(file_desc, IOCTL_STRELA_CONFIG) != 0)
    {
        printf("ERROR: Transfering config to the device!\n");
        goto error;
    }

    uint64_t end_cgra_config = micros();

    // Execute

    printf("Executing...\n");

    uint64_t begin_cgra_exec = micros();

    if (ioctl(file_desc, IOCTL_STRELA_EXEC) != 0)
    {
        printf("ERROR: Timeout while executing!\n");
        goto error;
    }

    uint64_t end_cgra_exec = micros();

    printf("Copying results from the driver to the user buffer...\n");

    // Copy output data from buffer
    uint64_t begin_read_result = micros();

    memcpy(output_data, (mmap_ptr + OUTPUT_OFFSET), TRANSFER_SIZE * sizeof(uint32_t));

    uint64_t end_read_result = micros();

    printf("Running pure software implementation (without using the accelerator)...\n");

    uint64_t begin_sw = micros();

    for(int i = 0; i< TRANSFER_SIZE; i++)
    {
        output_data_sw[i] = input_data[i] > 0 ? input_data[i] : 0;
    }

    uint64_t end_sw = micros();

    printf("Input -----------\n");
    examine_mem(mmap_ptr, INPUT_OFFSET, 20);

    printf("Output CGRA -----------\n");
    examine_mem(mmap_ptr, OUTPUT_OFFSET, 20);

    unsigned total_cgra = 0;
    unsigned delta_cycles;

    delta_cycles = US_TO_CYCLES(end_write_input - begin_write_input);
    printf("Write input    : %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_write_config - begin_write_config);
    printf("Write config   : %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_setup_transf - begin_setup_transf);
    printf("Setup transf   : %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cgra_config - begin_cgra_config);
    printf("Config         : %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cgra_exec - begin_cgra_exec);
    printf("Exec           : %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_read_result - begin_read_result);
    printf("Read result    : %d\n", delta_cycles); total_cgra += delta_cycles;

    printf("Total CGRA     : %d\n", total_cgra);

    delta_cycles = US_TO_CYCLES(end_sw - begin_sw);
    printf("CPU            : %d\n", delta_cycles);

    uint64_t a, b;
    a = micros();
    b = micros();
    printf("Min            : %d \n", US_TO_CYCLES(b - a));

    munmap(mmap_ptr, STRELA_DATA_REGION_SIZE);
    close(file_desc);

    return;

error:
    munmap(mmap_ptr, STRELA_DATA_REGION_SIZE);
    close(file_desc);

    exit(EXIT_FAILURE);
}

// Copyright 2025 CEI - UPM.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Juan Granja <juan.granja@upm.es>
// Milos Dordevic <milos.dordevic@upm.es>

#include "strela.h" 
#include "cma.h"

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

#define US_TO_CYCLES(t) (unsigned int)((t) * 1e-6 * 50e6)

//#define TRANSFER_SIZE (8192) // 32 KB
 #define TRANSFER_SIZE (4096) // 16 KB

int32_t input_data_sw[TRANSFER_SIZE];
int32_t output_data_sw[TRANSFER_SIZE];

#define RELU_KRNL_NPE (14)
#define RELU_KRNL_SIZE (RELU_KRNL_NPE * 5)
#define RELU_KRNL_BYTES (RELU_KRNL_SIZE * 4)

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
    int32_t *input = NULL;
    int32_t *result = NULL;
    int32_t *conf = NULL;

    int file_desc_strela;
    
    file_desc_strela = open("/dev/strela1", O_RDWR);

    if (file_desc_strela < 0) {
        printf("Can't open device file: %s, error:%d\n", "/dev/strela1", file_desc_strela);
        goto error;
    }

    printf("\n---------\n");

    input = cma_alloc(TRANSFER_SIZE * sizeof(int32_t), "strela1");

    if(!input)
    {
        goto error_mem_alloc;
    }

    result = cma_alloc(TRANSFER_SIZE * sizeof(int32_t), "strela1");

    if(!result)
    {
        goto error_mem_alloc_res;
    }

    conf = cma_alloc(RELU_KRNL_SIZE * sizeof(uint32_t), "strela1");

    if(!conf)
    {
        goto error_mem_alloc_conf;
    }

    // Populate input data
    for(int i = 0; i < TRANSFER_SIZE; i++)
    {
        input[i] = i % 2 ? i : -i;
    }

    for(int i = 0; i < TRANSFER_SIZE; i++)
    {
        input_data_sw[i] = i % 2 ? i : -i;
    }

    // Read input data befor write (test cache flushing)
    printf("OUTPUT before (first 20 elements):\n");

    for(int i = 0; i < 20; i++)
    {
        result[i] = 0xffffffff;
    }

    examine_mem(result, 0, 20);

    // Copy config to buffer
    uint32_t *cgra_kernel = relu_kernel;
    uint32_t cgra_kernel_size_words = RELU_KRNL_SIZE;

    printf("Copying config...\n");

    uint64_t begin_write_config = micros();

    memcpy(conf, cgra_kernel, cgra_kernel_size_words * sizeof(uint32_t));

    uint64_t end_write_config = micros();

    printf("Setting up config transfer...\n");

    uint64_t begin_cfg_setup_transf = micros();

    struct strela_ctrl cgra_ctrl = {0};

    cgra_ctrl.in_buf_id = cma_get_buff_id(conf);
    cgra_ctrl.out_buf_id = cma_get_buff_id(conf);
    
    cgra_ctrl.csrs.conf_offs = 0;
    cgra_ctrl.csrs.conf_count = cgra_kernel_size_words;

    if (ioctl(file_desc_strela, IOCTL_STRELA_CONTROL, &cgra_ctrl) != 0)
    {
        printf("ERROR: Setting up config transfer!\n");
        goto error_strela_ioctl;
    }

    uint64_t end_cfg_setup_transf = micros();

    // Configure 1

    printf("Transfering config to the device...\n");

    uint64_t begin_cgra_config = micros();

    if (ioctl(file_desc_strela, IOCTL_STRELA_CONFIG) != 0)
    {
        printf("ERROR: Transfering config to the device!\n");
        goto error_strela_ioctl;
    }

    uint64_t end_cgra_config = micros();

    printf("Setting up transfer...\n");

    uint64_t begin_setup_transf = micros();

    cgra_ctrl.in_buf_id = cma_get_buff_id(input);
    cgra_ctrl.out_buf_id = cma_get_buff_id(result);
    
    cgra_ctrl.csrs.conf_offs = 0;
    cgra_ctrl.csrs.conf_count = 0;

    cgra_ctrl.csrs.in0_offs = 0;
    cgra_ctrl.csrs.in0_count = TRANSFER_SIZE;
    cgra_ctrl.csrs.in0_stride = 4;

    cgra_ctrl.csrs.out0_offs = 0;
    cgra_ctrl.csrs.out0_count = TRANSFER_SIZE;

    if (ioctl(file_desc_strela, IOCTL_STRELA_CONTROL, &cgra_ctrl) != 0)
    {
        printf("ERROR: Setting up transfer!\n");
        goto error_strela_ioctl;
    }

    uint64_t end_setup_transf = micros();

    // Execute
    printf("Executing...\n");

    uint64_t begin_cgra_exec = micros();

    if (ioctl(file_desc_strela, IOCTL_STRELA_EXEC) != 0)
    {
        printf("ERROR: Timeout while executing!\n");
        goto error_strela_ioctl;
    }

    uint64_t end_cgra_exec = micros();

    printf("Running pure software implementation (without using the accelerator)...\n");

    uint64_t begin_sw = micros();

    for(int i = 0; i< TRANSFER_SIZE; i++)
    {
        output_data_sw[i] = input_data_sw[i] > 0 ? input_data_sw[i] : 0;
    }

    uint64_t end_sw = micros();

    printf("Input (first 20 elements) -----------\n");
    examine_mem(input, 0, 20);

    printf("Output CGRA (first 20 elements) -----------\n");
    examine_mem(result, 0, 20);

    printf("Output SW (CPU) (first 20 elements) -----------\n");
    examine_mem(output_data_sw, 0, 20);

    unsigned total_cgra = 0;
    unsigned delta_cycles;

    delta_cycles = US_TO_CYCLES(end_write_config - begin_write_config);
    printf("Write config (cycles): %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cfg_setup_transf - begin_cfg_setup_transf);
    printf("Setup config transfer (cycles): %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cgra_config - begin_cgra_config);
    printf("Config (cycles): %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_setup_transf - begin_setup_transf);
    printf("Setup transfer (cycles): %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cgra_exec - begin_cgra_exec);
    printf("Execute (cycles): %d\n", delta_cycles); total_cgra += delta_cycles;

    printf("Total CGRA (cycles): %d\n", total_cgra);

    delta_cycles = US_TO_CYCLES(end_sw - begin_sw);
    printf("CPU (cycles): %d\n", delta_cycles);

    uint64_t a, b;
    a = micros();
    b = micros();
    printf("Min (cycles): %d\n", US_TO_CYCLES(b - a));

    cma_free(input);
    cma_free(result);
    cma_free(conf);
    
    close(file_desc_strela);
    
    return;

error_strela_ioctl:
error_mem_alloc_conf:
    cma_free(result);
error_mem_alloc_res:
    cma_free(input);
error_mem_alloc:
    close(file_desc_strela);
error:
    exit(EXIT_FAILURE);
}

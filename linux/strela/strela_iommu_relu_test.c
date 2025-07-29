// Copyright 2025 CEI - UPM.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Juan Granja <juan.granja@upm.es>
// Milos Dordevic <milos.dordevic@upm.es>

#include "strela.h" 
#include "accel_dyn_cma.h" 
 
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
    uint32_t *mmap_ptr_input = NULL;
    uint32_t *mmap_ptr_result = NULL;
    uint32_t *mmap_ptr_conf = NULL;

    int file_desc_strela;
    int file_desc_alloc;
    
    file_desc_strela = open(DEVICE_PATH, O_RDWR);

    if (file_desc_strela < 0) {
        printf("Can't open device file: %s, error:%d\n", DEVICE_PATH, file_desc_strela);
        goto error;
    }

    file_desc_alloc = open(ACCEL_DYN_CMA_DEV_NAME, O_RDWR);

    if (file_desc_alloc < 0) {
        printf("Can't open device file: %s, error:%d\n", ACCEL_DYN_CMA_DEV_NAME, file_desc_alloc);
        goto error_alloc_open;
    }

    printf("\n---------\n");

    struct accel_dyn_cma_alloc_req_ioctl_arg buf_args_input = {
        .size = TRANSFER_SIZE * sizeof(int32_t),
        .id = -1,
    };

    memset(buf_args_input.dev_name, 0, sizeof(buf_args_input.dev_name));
    memcpy(buf_args_input.dev_name, "1002000.strela", sizeof("1002000.strela") - 1);

    struct accel_dyn_cma_alloc_req_ioctl_arg buf_args_output = {
        .size = TRANSFER_SIZE * sizeof(int32_t),
        .id = -1,
    };

    memset(buf_args_output.dev_name, 0, sizeof(buf_args_output.dev_name));
    memcpy(buf_args_output.dev_name, "1002000.strela", sizeof("1002000.strela") - 1);

    struct accel_dyn_cma_alloc_req_ioctl_arg buf_args_cfg = {
        .size = RELU_KRNL_SIZE * sizeof(uint32_t),
        .id = -1,
    };

    memset(buf_args_cfg.dev_name, 0, sizeof(buf_args_cfg.dev_name));
    memcpy(buf_args_cfg.dev_name, "1002000.strela", sizeof("1002000.strela") - 1);

    if(ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_ALLOC, &buf_args_input) != 0)
    {
        printf("ERROR: Failed to allocate buffer of size: %d bytes!\n", buf_args_input.size);
        goto error_input_alloc;
    }

    printf("Allocated buffer with size: %d bytes and ID: %d\n", buf_args_input.size, buf_args_input.id);

    if(ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_ALLOC, &buf_args_output) != 0)
    {
        printf("ERROR: Failed to allocate buffer of size: %d bytes!\n", buf_args_output.size);
        goto error_output_alloc;
    }

    printf("Allocated buffer with size: %d bytes and ID: %d\n", buf_args_output.size, buf_args_output.id);

    if(ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_ALLOC, &buf_args_cfg) != 0)
    {
        printf("ERROR: Failed to allocate buffer of size: %d bytes!\n", buf_args_cfg.size);
        goto error_cfg_alloc;
    }

    printf("Allocated buffer with size: %d bytes and ID: %d\n", buf_args_cfg.size, buf_args_cfg.id);

    char buf_dev_name[32];

    snprintf(buf_dev_name, sizeof(buf_dev_name), "/dev/udmabuf%d", buf_args_cfg.id);

    int buf_cfg_fd = open(buf_dev_name, O_RDWR);

    if (buf_cfg_fd < 0) {
        printf("Can't open device file: %s, error:%d\n", buf_dev_name, buf_cfg_fd);
        goto error_mmap;
    }

    // void *mmap(void addr[.length], size_t length, int prot, int flags, int fd, off_t offset);
    // NULL: Kernel chooses address
    mmap_ptr_conf = mmap(NULL, buf_args_cfg.size, PROT_READ | PROT_WRITE, MAP_SHARED, buf_cfg_fd, 0);

    if (mmap_ptr_conf == MAP_FAILED)
    {
        printf("mmap for config buffer failed\n");
        goto error_close_fd_buf_cfg;
    }

    snprintf(buf_dev_name, sizeof(buf_dev_name), "/dev/udmabuf%d", buf_args_input.id);

    int buf_input_fd = open(buf_dev_name, O_RDWR);

    if (buf_input_fd < 0) {
        printf("Can't open device file: %s, error:%d\n", buf_dev_name, buf_input_fd);
        goto error_close_fd_buf_cfg;
    }

    // void *mmap(void addr[.length], size_t length, int prot, int flags, int fd, off_t offset);
    // NULL: Kernel chooses address
    mmap_ptr_input = mmap(NULL, buf_args_input.size, PROT_READ | PROT_WRITE, MAP_SHARED, buf_input_fd, 0);

    if (mmap_ptr_input == MAP_FAILED)
    {
        printf("mmap for input buffer failed\n");
        goto error_close_fd_buf_in;
    }

    snprintf(buf_dev_name, sizeof(buf_dev_name), "/dev/udmabuf%d", buf_args_output.id);
   
    int buf_output_fd = open(buf_dev_name, O_RDWR);

    if (buf_output_fd < 0) {
        printf("Can't open device file: %s, error:%d\n", buf_dev_name, buf_output_fd);
        goto error_close_fd_buf_in;
    }

    mmap_ptr_result = mmap(NULL, buf_args_output.size, PROT_READ | PROT_WRITE, MAP_SHARED, buf_output_fd, 0);

    if (mmap_ptr_result == MAP_FAILED)
    {
        printf("mmap for result buffer failed\n");
        goto error_close_fd_buf_out;
    }

    // Populate input data
    for(int i = 0; i < TRANSFER_SIZE; i++)
    {
        mmap_ptr_input[i] = i % 2 ? i : -i;
    }

    for(int i = 0; i < TRANSFER_SIZE; i++)
    {
        input_data_sw[i] = i % 2 ? i : -i;
    }

    // Read input data befor write (test cache flushing)
    printf("OUTPUT before (first 20 elements):\n");

    for(int i = 0; i < 20; i++)
    {
        mmap_ptr_result[i] = 0xffffffff;
    }

    examine_mem(mmap_ptr_result, 0, 20);

    // Copy config to buffer
    uint32_t *cgra_kernel = relu_kernel;
    uint32_t cgra_kernel_size_words = RELU_KRNL_SIZE;

    printf("Copying config...\n");

    uint64_t begin_write_config = micros();

    memcpy(mmap_ptr_conf, cgra_kernel, cgra_kernel_size_words * sizeof(uint32_t));

    uint64_t end_write_config = micros();

    printf("Setting up config transfer...\n");

    uint64_t begin_cfg_setup_transf = micros();

    struct strela_ctrl cgra_ctrl = {0};

    cgra_ctrl.in_buf_id = buf_args_cfg.id;
    cgra_ctrl.out_buf_id = buf_args_cfg.id;
    
    cgra_ctrl.csrs.conf_offs = 0;
    cgra_ctrl.csrs.conf_count = cgra_kernel_size_words;

    if (ioctl(file_desc_strela, IOCTL_STRELA_CONTROL, &cgra_ctrl) != 0)
    {
        printf("ERROR: Setting up config transfer!\n");
        goto error_close_fd_buf_out;
    }

    uint64_t end_cfg_setup_transf = micros();

    // Configure 1

    printf("Transfering config to the device...\n");

    uint64_t begin_cgra_config = micros();

    if (ioctl(file_desc_strela, IOCTL_STRELA_CONFIG) != 0)
    {
        printf("ERROR: Transfering config to the device!\n");
        goto error_close_fd_buf_out;
    }

    uint64_t end_cgra_config = micros();

    printf("Setting up transfer...\n");

    uint64_t begin_setup_transf = micros();

    cgra_ctrl.in_buf_id = buf_args_input.id;
    cgra_ctrl.out_buf_id = buf_args_output.id;
    
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
        goto error_close_fd_buf_out;
    }

    uint64_t end_setup_transf = micros();

    // Execute
    printf("Executing...\n");

    uint64_t begin_cgra_exec = micros();

    if (ioctl(file_desc_strela, IOCTL_STRELA_EXEC) != 0)
    {
        printf("ERROR: Timeout while executing!\n");
        goto error_close_fd_buf_out;
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
    examine_mem(mmap_ptr_input, 0, 20);

    printf("Output CGRA (first 20 elements) -----------\n");
    examine_mem(mmap_ptr_result, 0, 20);

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

    munmap(mmap_ptr_input, buf_args_input.size);
    munmap(mmap_ptr_result, buf_args_output.size);
    munmap(mmap_ptr_conf, buf_args_cfg.size);

    close(buf_cfg_fd);
    close(buf_input_fd);
    close(buf_output_fd);

    if(ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_FREE, &buf_args_cfg.id) != 0)
    {
        printf("ERROR: Failed to deallocate buffer with ID: %d!\n", buf_args_cfg.id);
        goto error_cfg_alloc;
    }
    
    if(ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_FREE, &buf_args_output.id) != 0)
    {
        printf("ERROR: Failed to deallocate buffer with ID: %d!\n", buf_args_output.id);
        goto error_output_alloc;
    }

    if(ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_FREE, &buf_args_input.id) != 0)
    {
        printf("ERROR: Failed to deallocate buffer with ID: %d!\n", buf_args_input.id);
        goto error_input_alloc;
    }

    close(file_desc_strela);
    close(file_desc_alloc);

    return;

error_close_fd_buf_out:
    munmap(mmap_ptr_result, buf_args_output.size);
error_close_fd_buf_in:
    munmap(mmap_ptr_input, buf_args_input.size);
error_close_fd_buf_cfg:
    munmap(mmap_ptr_conf, buf_args_cfg.size);
error_mmap:
    if(ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_FREE, &buf_args_input.id) != 0)
    {
        printf("ERROR: Failed to deallocate buffer with ID: %d!\n", buf_args_input.id);
    }
error_cfg_alloc:
    if(ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_FREE, &buf_args_output.id) != 0)
    {
        printf("ERROR: Failed to deallocate buffer with ID: %d!\n", buf_args_output.id);
    }
error_output_alloc:
    if(ioctl(file_desc_alloc, ACCEL_DYN_CMA_IOCTL_FREE, &buf_args_input.id) != 0)
    {
        printf("ERROR: Failed to deallocate buffer with ID: %d!\n", buf_args_input.id);
    }
error_input_alloc:
    munmap(mmap_ptr_input, buf_args_input.size);
    munmap(mmap_ptr_result, buf_args_output.size);
    munmap(mmap_ptr_conf, buf_args_cfg.size);

    close(file_desc_alloc);
error_alloc_open:
    close(file_desc_strela);
error:
    exit(EXIT_FAILURE);
}
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
#define INPUT_OFFSET    (0x10000/4)
#define OUTPUT_OFFSET   (0x20000/4)

#define US_TO_CYCLES(t) (unsigned int)((t) * 1e-6 * 50e6)

#define IMAGE_SIDE 32
#define DATA_SIZE (IMAGE_SIDE * IMAGE_SIDE) // 1024
#define RESULT_SIZE (IMAGE_SIDE * IMAGE_SIDE)

/*
// Input 32x32
// Output: 32x32 but external 1-pixel boundary is to be ignored
// To do it in one pass, start at (x, y): (1,1) end at (n-2, n-2)
// Number of result: (n*n) -n -n -2
// Offset of result: n+1
*/

#define WRITE_RESULT_SIZE   (IMAGE_SIDE * IMAGE_SIDE - IMAGE_SIDE - IMAGE_SIDE - 2)
#define WRITE_RESULT_OFFSET (IMAGE_SIDE + 1)

#define CONV2D_1_KRNL_NPE 8
#define CONV2D_1_KRNL_SIZE CONV2D_1_KRNL_NPE * 5
#define CONV2D_1_KRNL_BYTES CONV2D_1_KRNL_SIZE * 4

extern uint32_t conv2d_1_kernel[CONV2D_1_KRNL_SIZE];

#define CONV2D_2_KRNL_NPE 10
#define CONV2D_2_KRNL_SIZE CONV2D_2_KRNL_NPE * 5
#define CONV2D_2_KRNL_BYTES CONV2D_2_KRNL_SIZE * 4

static uint32_t *mmap_ptr = NULL;

uint32_t conv2d_1_kernel[CONV2D_1_KRNL_SIZE] = {
    0xC0000020, 0x00202200, 0x00000000, 0x00000000, 0xC5000000, // 0
    0xC0000020, 0x00202200, 0x00000000, 0xFFFFFFFF, 0xC5010000, // 1
    0xC0000020, 0x00202200, 0x00000000, 0x00000000, 0xC5020000, // 2

    0x00000004, 0x00000000, 0x00000000, 0x00000000, 0x05040000, // 4
    0x18800010, 0x00400030, 0x00000000, 0x00000000, 0xE5050000, // 5
    0xC0800010, 0x00200030, 0x00000000, 0x00000000, 0xE5060000, // 6

    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x050A0000, // 10

    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x050E0000  // 14
};

uint32_t conv2d_2_kernel[CONV2D_2_KRNL_SIZE] = {
    0xC0000020, 0x00202200, 0x00000000, 0x00000000, 0xC5000000, // 0
    0xC0000020, 0x00202200, 0x00000000, 0xFFFFFFFF, 0xC5010000, // 1
    0xC0000020, 0x00202200, 0x00000000, 0x00000000, 0xC5020000, // 2
    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x05030000, // 3

    0x00000004, 0x00000000, 0x00000000, 0x00000000, 0x05040000, // 4
    0x18800010, 0x00400030, 0x00000000, 0x00000000, 0xE5050000, // 5
    0x18800010, 0x00400030, 0x00000000, 0x00000000, 0xE5060000, // 6
    0xC0800010, 0x00200030, 0x00000000, 0x00000000, 0xE5070000, // 7

    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x050B0000, // 11

    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x050F0000  // 15
};

static int32_t image[DATA_SIZE] =
{
    12, 88, 59, 42, 14, 96, 65, 59, 82, 87, 71, 87, 99, 89, 79, 20, 45,  4,  8, 38, 30, 97, 26, 28, 78, 24, 43,  4, 65, 52, 16, 94,
    87, 95, 11, 98, 36, 44, 52, 89, 95, 87, 55, 64, 28, 47, 11, 67, 87, 67, 49, 22, 66, 95, 65, 25, 64, 77, 34, 75, 13, 86, 33, 27,
    14, 87, 70, 96, 60,  0,  2, 72,  1, 25, 87, 94,  6, 50, 12, 51, 54, 58, 71, 29, 37, 80, 45, 99, 64, 82, 18, 85, 68, 51, 62, 65,
    55, 85, 29, 19, 66, 24, 13,  1, 13, 93, 37, 29, 11, 43, 95, 90, 35, 87, 65, 39, 16, 89, 52, 62, 16, 84, 17, 23, 79, 44, 62, 92,
    57, 23, 68, 75, 56, 41, 84, 92, 70, 95, 88, 15, 16, 37,  9, 28, 31, 97, 69, 80, 61, 93, 73, 69, 19, 37, 15, 48, 72, 80, 74, 91,
    74, 48, 80, 98, 72, 30, 47, 15,  7, 12,  4, 34, 24,  7, 22,  3, 69, 65, 51, 34, 60, 10, 90,  0, 37, 92, 48, 97, 40, 60, 93, 73,
    58, 70, 42, 41, 48,  8, 65, 21, 40, 85, 73, 82, 46,  6, 99, 13, 27,  6, 85, 17, 26, 22, 77,  3, 30, 86, 33, 22, 63, 21, 58, 49,
    42, 82, 97, 39, 52, 85, 62, 35,  1, 71,  1, 76, 85, 12,  8, 91, 67, 56, 66, 30, 54, 55, 38, 15, 53, 55, 78, 37, 52, 87, 29, 55,
    43, 65, 92, 51,  7, 91, 67, 76, 95, 67, 39, 30, 71, 64, 92, 63, 45, 15, 70, 33, 20, 14, 52, 97, 97, 69, 89, 97, 73, 91, 47, 51,
    28, 18, 25, 35, 49,  7,  7, 20, 81,  4, 81,  4, 46, 49, 98, 25, 94, 16, 98, 36, 87, 83, 41, 40, 66, 23, 50, 88, 78, 92, 23, 23,
    24, 42,  9, 82, 79, 67, 72, 70,  4,  0, 68, 43, 28, 86, 77, 26, 39, 88, 47, 99, 23, 16, 27, 94, 57, 89, 30, 56, 90, 62, 30, 12,
    97, 72, 41, 29, 95, 86, 55, 14, 47, 18, 42, 89, 18, 65, 61, 82,  0, 67,  0, 80, 92, 72, 59, 92, 43, 40,  5, 74, 90, 18, 62, 69,
    63, 68, 21, 12,  6, 17, 57, 68, 13, 39, 78, 17, 43, 31, 59, 95, 44, 96, 19, 16, 84, 92, 10, 83, 57,  8, 58, 41, 10, 84, 71, 77,
    64, 92, 43, 74, 64,  2, 65, 10, 84,  0, 74, 13, 73, 30, 25, 85, 70, 16, 39, 30, 49, 17, 55, 57, 67, 29, 23, 34, 45, 48, 80, 26,
    56, 71, 79, 65, 94,  2, 35, 75, 73, 95, 63, 17, 64, 41, 31,  2, 67,  1, 89, 93, 19, 99,  8, 39, 65, 99, 81, 38, 50, 55, 92, 24,
    75,  3, 96, 45, 45, 11,  8, 62, 65, 98, 25, 55, 26, 60, 45, 39, 67, 65, 55, 24, 50, 25, 39, 52, 98, 37, 56, 31, 53, 25, 84, 20,
    27, 93, 31,  1, 68,  0, 51, 48, 34, 60, 77, 69, 84, 67, 83, 46, 52, 60, 18, 20, 23, 94, 27, 36, 11, 30, 24, 52, 46, 59, 43, 66,
    76,  2, 78, 84, 81, 92, 53, 85, 84, 17, 16, 82, 46, 74, 57, 16, 15, 97, 34, 14, 58, 51, 98, 67, 32, 69, 12, 65, 60, 29, 18,  3,
    68, 80, 35, 15, 10, 20, 46, 84, 45,  2,  2, 18, 50, 24,  9, 51, 70, 68, 93, 43, 46,  7, 88, 40, 40,  2, 22, 61, 52, 53, 76, 10,
    59,  2, 61, 48, 90, 95, 21, 65, 28, 45, 31, 88, 17, 16, 59, 19, 64, 32, 97, 81, 49, 36, 81, 98, 25, 29, 98, 43, 62, 98, 49, 47,
    58, 44, 32, 44, 88, 66, 56, 34, 11, 32, 72, 79, 77, 14, 73, 77, 27, 13, 28, 36, 53,  3,  4, 35,  4, 34, 98, 30, 88, 75, 86, 62,
    28, 76, 51, 76, 78, 17, 60, 68, 91, 77, 56, 39,  9, 38, 85, 42, 76, 29, 50, 69, 23, 83,  6, 31, 94, 64, 15, 86, 87, 12, 22,  8,
    30, 39, 97, 97, 49, 38, 52, 87, 31, 36, 26, 32, 87,  6, 36, 47, 15, 77, 86, 99, 91, 45, 33, 14, 22, 46, 18, 17, 47, 12, 67, 43,
    71, 10, 59, 88,  3, 42, 25, 14, 92, 67, 82, 44, 89, 81, 94, 92, 74, 22, 78,  0, 71, 42, 78, 39, 98, 73, 76,  9, 53, 64,  9, 27,
    69, 54, 54,  2, 99,  3, 14, 96, 61, 31, 16, 97, 34, 66, 46, 19, 70, 26, 70, 86,  8, 68,  6,  5, 38, 13, 31, 29, 63, 12, 98, 45,
    34, 19, 70, 20, 91, 68, 36, 56, 19, 29, 20, 84, 73, 62, 80, 63, 39,  3,  1, 12, 96,  4, 77,  6, 51, 55,  3,  0, 38, 69, 71, 97,
    99, 24, 44, 11, 19, 60, 98, 19, 30, 74, 27, 76, 64, 61, 62, 55,  0, 32, 44, 82, 64, 84, 40, 88, 58, 43,  8, 59, 30, 56,  0, 75,
    13, 22, 35, 79, 88, 36, 78, 27, 60, 87,  6, 29, 34,  9, 81, 50, 48,  4, 83, 29, 17, 84, 75, 16, 98, 77, 61, 53, 17, 93, 97, 29,
    38,  7, 36, 34,  9, 72,  5, 48, 82, 25, 46, 77, 35,  8, 87, 48, 13, 27, 52, 29, 65, 19, 54, 27, 39, 70, 53, 40, 27,  6,  5, 82,
    49, 17, 83, 18, 35, 19, 49, 92, 46, 28, 93, 94, 92, 40, 44, 10, 83, 50, 57, 48, 14, 46, 12, 55, 72, 56, 92, 84, 98, 10, 31, 23,
    40, 92, 72,  5, 13, 19, 11, 71, 94, 91, 75, 70, 64, 30, 30, 84, 11, 32, 65, 86, 36, 55, 78,  5, 83,  3, 62, 83, 61, 97, 87, 97,
    44, 87, 95, 64, 55,  6, 14, 49,  5, 88, 29,  1, 10, 60, 89, 83, 94, 19, 47, 58, 16, 33,  3, 26, 89, 72, 63, 21, 75, 85, 36, 94,
};

static uint32_t result[DATA_SIZE];
static uint32_t result_sw[DATA_SIZE];

void test_conv2d();

int main(void) 
{ 
    test_conv2d();

    return 0;
}

void test_conv2d()
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

    // Read input data befor write (Test cache flushing)
    
    printf("OUTPUT before:\n");

    for(int i = 0; i< 20; i++)
    {
        *(mmap_ptr + OUTPUT_OFFSET + i) = 0xffffffff;
    }
    
    examine_mem(mmap_ptr, OUTPUT_OFFSET, 20);

    // Copy image to buffer
    uint64_t begin_write_image = micros();

    memcpy((mmap_ptr + INPUT_OFFSET), image, DATA_SIZE * sizeof(uint32_t));

    uint64_t end_write_image = micros();

    // Copy config 1 to buffer

    printf("Copying config 1...\n");

    uint64_t begin_write_config1 = micros();

    uint32_t *cgra_kernel = conv2d_1_kernel;
    uint32_t cgra_kernel_size_words = CONV2D_1_KRNL_SIZE;

    memcpy((mmap_ptr + CONFIG_OFFSET), cgra_kernel, cgra_kernel_size_words * sizeof(uint32_t));

    uint64_t end_write_config1 = micros();

    // Setup transfer 1

    printf("Setting up transfer 1...\n");

    uint64_t begin_setup_transf1 = micros();

    struct strela_csrs cgra_ctrl1 = {0};

    cgra_ctrl1.conf_offs = CONFIG_OFFSET;
    cgra_ctrl1.conf_count = cgra_kernel_size_words;

    cgra_ctrl1.in0_offs = INPUT_OFFSET + 0;
    cgra_ctrl1.in0_count = WRITE_RESULT_SIZE;
    cgra_ctrl1.in0_stride = 4;

    cgra_ctrl1.in1_offs = INPUT_OFFSET + 1;
    cgra_ctrl1.in1_count = WRITE_RESULT_SIZE;
    cgra_ctrl1.in1_stride = 4;

    cgra_ctrl1.in2_offs = INPUT_OFFSET + 2;
    cgra_ctrl1.in2_count = WRITE_RESULT_SIZE;
    cgra_ctrl1.in2_stride = 4;

    cgra_ctrl1.out2_offs = OUTPUT_OFFSET + WRITE_RESULT_OFFSET;
    cgra_ctrl1.out2_count = WRITE_RESULT_SIZE;

    if (ioctl(file_desc, IOCTL_STRELA_CONTROL, &cgra_ctrl1) != 0)
    {
        printf("ERROR: Setting up transfer 1!\n");
        goto error;
    }

    uint64_t end_setup_transf1 = micros();

    // Configure 1

    printf("Transfering config 1 to the device...\n");

    uint64_t begin_cgra_config1 = micros();

    if (ioctl(file_desc, IOCTL_STRELA_CONFIG) != 0)
    {
        printf("ERROR: Transfering config 1 to the device!\n");
        goto error;
    }

    uint64_t end_cgra_config1 = micros();

    // Execute 1

    printf("Executing part 1...\n");

    uint64_t begin_cgra_exec1 = micros();

    if (ioctl(file_desc, IOCTL_STRELA_EXEC) != 0)
    {
        printf("ERROR: Timeout while executing part 1!\n");
        goto error;
    }

    uint64_t end_cgra_exec1 = micros();

    // Copy config 2 to buffer

    printf("Copying config 2...\n");

    uint64_t begin_write_config2 = micros();

    cgra_kernel = conv2d_2_kernel;
    cgra_kernel_size_words = CONV2D_2_KRNL_SIZE;

    memcpy((mmap_ptr + CONFIG_OFFSET), cgra_kernel, cgra_kernel_size_words * sizeof(uint32_t));

    uint64_t end_write_config2 = micros();

    // Setup transfer 2

    printf("Setting up transfer 2...\n");

    uint64_t begin_setup_transf2 = micros();

    struct strela_csrs cgra_ctrl2 = {0};

    cgra_ctrl2.conf_offs = CONFIG_OFFSET;
    cgra_ctrl2.conf_count = cgra_kernel_size_words;

    cgra_ctrl2.in0_offs = INPUT_OFFSET + (IMAGE_SIDE + 0);
    cgra_ctrl2.in0_count = WRITE_RESULT_SIZE;
    cgra_ctrl2.in0_stride = 4;

    cgra_ctrl2.in1_offs = INPUT_OFFSET + (IMAGE_SIDE + 1);
    cgra_ctrl2.in1_count = WRITE_RESULT_SIZE;
    cgra_ctrl2.in1_stride = 4;

    cgra_ctrl2.in2_offs = INPUT_OFFSET + (IMAGE_SIDE + 2);
    cgra_ctrl2.in2_count = WRITE_RESULT_SIZE;
    cgra_ctrl2.in2_stride = 4;

    cgra_ctrl2.in3_offs = OUTPUT_OFFSET + WRITE_RESULT_OFFSET;
    cgra_ctrl2.in3_count = WRITE_RESULT_SIZE;
    cgra_ctrl2.in3_stride = 4;

    cgra_ctrl2.out3_offs = OUTPUT_OFFSET + WRITE_RESULT_OFFSET;
    cgra_ctrl2.out3_count = WRITE_RESULT_SIZE;

    if (ioctl(file_desc, IOCTL_STRELA_CONTROL, &cgra_ctrl2) != 0)
    {
        printf("ERROR: Setting up transfer 2!\n");
        goto error;
    }

    uint64_t end_setup_transf2 = micros();

    // Configure 2

    printf("Transfering config 2 to the device...\n");

    uint64_t begin_cgra_config2 = micros();

    if (ioctl(file_desc, IOCTL_STRELA_CONFIG) != 0)
    {
        printf("ERROR: Transfering config 2 to the device!\n");
        goto error;
    }

    uint64_t end_cgra_config2 = micros();

    // Execute 2

    printf("Executing part 2...\n");

    uint64_t begin_cgra_exec2 = micros();

    if (ioctl(file_desc, IOCTL_STRELA_EXEC) != 0)
    {
        printf("ERROR: Timeout while executing part 2!\n");
        goto error;
    }

    uint64_t end_cgra_exec2 = micros();

    // Setup transfer 3

    printf("Setting up transfer 3...\n");

    uint64_t begin_setup_transf3 = micros();

    struct strela_csrs cgra_ctrl3 = { 0 };

    cgra_ctrl3.conf_offs = CONFIG_OFFSET;
    cgra_ctrl3.conf_count = cgra_kernel_size_words;

    cgra_ctrl3.in0_offs = INPUT_OFFSET + (2 * IMAGE_SIDE + 0);
    cgra_ctrl3.in0_count = WRITE_RESULT_SIZE;
    cgra_ctrl3.in0_stride = 4;

    cgra_ctrl3.in1_offs = INPUT_OFFSET + (2 * IMAGE_SIDE + 1);
    cgra_ctrl3.in1_count = WRITE_RESULT_SIZE;
    cgra_ctrl3.in1_stride = 4;

    cgra_ctrl3.in2_offs = INPUT_OFFSET + (2 * IMAGE_SIDE + 2);
    cgra_ctrl3.in2_count = WRITE_RESULT_SIZE;
    cgra_ctrl3.in2_stride = 4;

    cgra_ctrl3.in3_offs = OUTPUT_OFFSET + WRITE_RESULT_OFFSET;
    cgra_ctrl3.in3_count = WRITE_RESULT_SIZE;
    cgra_ctrl3.in3_stride = 4;

    cgra_ctrl3.out3_offs = OUTPUT_OFFSET + WRITE_RESULT_OFFSET;
    cgra_ctrl3.out3_count = WRITE_RESULT_SIZE;

    if (ioctl(file_desc, IOCTL_STRELA_CONTROL, &cgra_ctrl3) != 0)
    {
        printf("ERROR: Setting up transfer 3!\n");
        goto error;
    }

    uint64_t end_setup_transf3 = micros();

    // Configure 3 - Same as 2, not needed
    printf("NOTE: config 3 same as config 2!\n");

    printf("Executing part 3...\n");

    // Execute 3
    uint64_t begin_cgra_exec3 = micros();

    if (ioctl(file_desc, IOCTL_STRELA_EXEC) != 0)
    {
        printf("ERROR: Timeout while executing part 3!\n");
        goto error;
    }

    uint64_t end_cgra_exec3 = micros();

    printf("Copying results from the driver to the user buffer...\n");

    // Copy output data from buffer
    uint64_t begin_read_result = micros();

    memcpy(result, (mmap_ptr + OUTPUT_OFFSET), DATA_SIZE * sizeof(uint32_t));

    uint64_t end_read_result = micros();

    // Software implementation
    int32_t filter[] = { 0, -1, 0, \
                        0, -1, 0, \
                        0, -1, 0, };

    printf("Running pure software implementation (without using the accelerator)...\n");
    uint64_t begin_sw = micros();

    for(int i = 1; i < IMAGE_SIDE - 1; i++) {
        for(int j = 1; j < IMAGE_SIDE - 1; j++) {
            result_sw[(i) * IMAGE_SIDE + j] = \

            filter[0] * image[(i - 1) * IMAGE_SIDE + j - 1] + filter[1] * image[(i - 1) * IMAGE_SIDE + j + 0] + filter[2] * image[(i - 1) * IMAGE_SIDE + j + 1] +
            filter[3] * image[(i + 0) * IMAGE_SIDE + j - 1] + filter[4] * image[(i + 0) * IMAGE_SIDE + j + 0] + filter[5] * image[(i + 0) * IMAGE_SIDE + j + 1] +
            filter[6] * image[(i + 1) * IMAGE_SIDE + j - 1] + filter[7] * image[(i + 1) * IMAGE_SIDE + j + 0] + filter[8] * image[(i + 1) * IMAGE_SIDE + j + 1];
        }
    }

    uint64_t end_sw = micros();

    printf("STRELA:\n");
    examine_mem(mmap_ptr, OUTPUT_OFFSET + IMAGE_SIDE + 1, 20);

    printf("CPU:\n");
    examine_mem(result_sw, IMAGE_SIDE + 1, 20);

    unsigned total_cgra = 0;
    unsigned delta_cycles;

    delta_cycles = US_TO_CYCLES(end_write_image - begin_write_image);
    printf("Write image: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_write_config1 - begin_write_config1);
    printf("Write config 1: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_setup_transf1 - begin_setup_transf1);
    printf("Setup transf 1: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cgra_config1 - begin_cgra_config1);
    printf("Config 1: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cgra_exec1 - begin_cgra_exec1);
    printf("Exec 1: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cgra_config2 - begin_cgra_config2);
    printf("Write config 2: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_setup_transf2 - begin_setup_transf2);
    printf("Setup transf 2: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cgra_config2 - begin_cgra_config2);
    printf("Config 2: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cgra_exec2 - begin_cgra_exec2);
    printf("Exec 2: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_setup_transf3 - begin_setup_transf3);
    printf("Setup transf 3: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_cgra_exec3 - begin_cgra_exec3);
    printf("Exec 3: %d\n", delta_cycles); total_cgra += delta_cycles;

    delta_cycles = US_TO_CYCLES(end_read_result - begin_read_result);
    printf("Read result: %d\n", delta_cycles); total_cgra += delta_cycles;

    printf("Total STRELA: %d\n", total_cgra);

    delta_cycles = US_TO_CYCLES(end_sw - begin_sw);
    printf("CPU: %d\n", delta_cycles);

    uint64_t a, b;
    a = micros();
    b = micros();
    printf("Min: %d \n", US_TO_CYCLES(b - a));

    munmap(mmap_ptr, STRELA_DATA_REGION_SIZE);
    close(file_desc);

    exit(EXIT_SUCCESS);

error:
    munmap(mmap_ptr, STRELA_DATA_REGION_SIZE);
    close(file_desc);

    exit(EXIT_FAILURE);
}

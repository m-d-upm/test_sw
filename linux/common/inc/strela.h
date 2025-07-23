/*
 * Driver for the STRELA CGRA with embedded DMA
 *
 * Copyright (C) 2025 Juan Granja, CEI-UPM.
 * Copyright (C) 2025 Milos Dordevic, CEI-UPM.
 */

#ifndef _STRELA_H_
#define _STRELA_H_

#include <stdint.h>

#define DEVICE_PATH ("/dev/strela")

#define STRELA_CTRL_BIT_START_EXEC    	(0x1U)
#define STRELA_CTRL_BIT_CLEAR_STATE   	(0x2U)

#define STRELA_CTRL_BIT_LOAD_CONFIG   	(0x4U)
#define STRELA_CTRL_BIT_CLEAR_CONFIG	(0x8U)

#define STRELA_CTRL_BIT_DONE_CONFIG	    (0x2U)
#define STRELA_CTRL_BIT_DONE_EXEC	    (0x1U)

#define STRELA_IN_BITS_STRIDE_COUNT(stride, count) ( (stride << 16U) | (stride * count) )
#define STRELA_OUT_BITS_STRIDE4_COUNT(count) ( count * 4U )

#define STRELA_CTRL_A 		    (0x00U)

#define STRELA_CONF_ADDR_A	    (0x04U)
#define STRELA_CONF_SIZE_A	    (0x08U)

#define STRELA_IN0_ADDR_A	    (0x10U)
#define STRELA_IN0_SIZE_A	    (0x14U)
#define STRELA_IN1_ADDR_A	    (0x18U)
#define STRELA_IN1_SIZE_A	    (0x1CU)
#define STRELA_IN2_ADDR_A	    (0x20U)
#define STRELA_IN2_SIZE_A	    (0x24U)
#define STRELA_IN3_ADDR_A	    (0x28U)
#define STRELA_IN3_SIZE_A	    (0x2CU)

#define STRELA_OUT0_ADDR_A	    (0x50U)
#define STRELA_OUT0_SIZE_A	    (0x54U)
#define STRELA_OUT1_ADDR_A	    (0x58U)
#define STRELA_OUT1_SIZE_A	    (0x5CU)
#define STRELA_OUT2_ADDR_A	    (0x60U)
#define STRELA_OUT2_SIZE_A	    (0x64U)
#define STRELA_OUT3_ADDR_A	    (0x68U)
#define STRELA_OUT3_SIZE_A	    (0x6CU)

#define STRELA_CNTR_CONF_A    	(0x90U)
#define STRELA_CNTR_EXEC_A    	(0x94U)
#define STRELA_CNTR_STALL_A   	(0x98U)

#define STRELA_OUT_ARB_HOLD_A 	(0xA0U)

#define STRELA_RESET_DMA_A      (0xF8U)

#define STRELA_AM_OPA 		    (0xF0U)
#define STRELA_AM_OPB 		    (0xF4U)
#define STRELA_AM_OPR 		    (0xF8U)

struct strela_csrs {
    uint32_t a;
    uint32_t b;

    uint32_t conf_offs;
    uint32_t conf_count;

    uint32_t in0_offs;
    uint32_t in0_count;
    uint32_t in0_stride;
    uint32_t in1_offs;
    uint32_t in1_count;
    uint32_t in1_stride;
    uint32_t in2_offs;
    uint32_t in2_count;
    uint32_t in2_stride;
    uint32_t in3_offs;
    uint32_t in3_count;
    uint32_t in3_stride;

    uint32_t out0_offs;
    uint32_t out0_count;
    uint32_t out1_offs;
    uint32_t out1_count;
    uint32_t out2_offs;
    uint32_t out2_count;
    uint32_t out3_offs;
    uint32_t out3_count;
};

struct strela_ctrl {
    int in_buf_id;
    int out_buf_id;
    struct strela_csrs csrs;
};

#define IOCTL_BASE 'W'

#define IOCTL_STRELA_CONTROL	_IOW(IOCTL_BASE, 1, struct strela_ctrl)
#define IOCTL_STRELA_CONFIG 	_IO(IOCTL_BASE, 2)
#define IOCTL_STRELA_EXEC	    _IO(IOCTL_BASE, 3)

#endif

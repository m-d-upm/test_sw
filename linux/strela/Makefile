# make sure that submodules for the MYRTUS project are initialized
# after which the Linux image needs to be created first using buildroot
# this will build the riscv64-buildroot-linux-gnu-gcc compiler as well
# so that it can be used by this Makefile to build user programs
CC = ../../../cheshire/sw/deps/cva6-sdk/buildroot/output/host/bin/riscv64-buildroot-linux-gnu-gcc

#CFLAGS = -Os -ggdb -Wall

INCLUDES = -I./ -I../common/inc/ -I../lib

SRCS_C = strela_iommu_test_main.c strela_iommu_relu_test.c strela_iommu_conv_test.c ../common/utilities.c ../lib/cma.c

OBJS_C = $(SRCS_C:.c=.o)

MAIN = prog.elf

#.PHONY: clean

$(MAIN): $(OBJS_C)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) $(OBJS_C) -o $(MAIN)
	@echo "LD    >= $(MAIN)"

%.o: %.c
	@$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@
	@echo "CC    <= $<"

%.o: %.S
	@$(CC) $(CFLAGS) $(CCASFLAGS) $(INCLUDES) -c $<  -o $@
	@echo "CC    <= $<"

clean:
	$(RM) $(OBJS_C) $(OBJS_S) $(MAIN)

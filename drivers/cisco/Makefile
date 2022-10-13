#!/bin/make -f
#
# Copyright (c) 2022 by Cisco Systems, Inc.
# All rights reserved.
#

ccflags-y += -std=gnu11 -I$(M)
ccflags-y += -DDEBUG_REG_TRACE=0

LONG_KVER := $(shell echo $$(($(VERSION)000000 + $(PATCHLEVEL)000 + $(SUBLEVEL))))
KVER_IS_LT_5_12_0 := $(shell [ $(LONG_KVER) -lt 5012000 ] && echo y)

ccflags-$(KVER_IS_LT_5_12_0) += -DI2C_FUNC_SMBUS_EMUL_ALL=I2C_FUNC_SMBUS_EMUL

obj-m += cisco-fpga-i2c.o
obj-m += cisco-fpga-i2c-ext.o
obj-m += cisco-fpga-info.o
obj-m += cisco-fpga-msd.o
obj-m += cisco-fpga-pseq.o

mfd-$(CONFIG_MFD_CORE) += cisco-fpga-bmc.o
obj-m += $(sort $(mfd-y) $(mfd-m))

gpio-$(CONFIG_GPIOLIB) += cisco-fpga-gpio.o
cisco-fpga-gpio-objs := \
    cisco-fpga-gpio-main.o \
    cisco-fpga-gpio-sysfs.o \
    cisco-fpga-gpio-init.o
obj-m += $(sort $(gpio-y) $(gpio-m))

obj-m += libcisco.o
libcisco-objs := \
    util.o \
    reg_trace.o \
    reg_access.o \
    hdr.o \
    mfd.o \
    msd_xil_sysfs.o \
    msd_xil_regmap.o \
    cisco-sysfs.o \
    cisco-acpi.o \
    cisco-reboot-notifier.o \
    i2c-arbitrate.o \
    i2c-arbitrate-sysfs.o
# For regmap/internal.h
CFLAGS_util.o += -Idrivers/base -Isource/drivers/base

obj-m += lattice_machx03.o

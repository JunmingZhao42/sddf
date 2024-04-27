#
# Copyright 2022, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#

ifeq ($(strip $(MICROKIT_SDK)),)
$(error MICROKIT_SDK must be specified)
endif

ifeq ($(strip $(MICROKIT_BOARD)),)
$(error MICROKIT_BOARD must be specified)
endif

TOOLCHAIN := aarch64-none-elf
QEMU := qemu-system-aarch64
CC := $(TOOLCHAIN)-gcc
LD := $(TOOLCHAIN)-ld
AS := $(TOOLCHAIN)-as

MICROKIT_TOOL := $(MICROKIT_SDK)/bin/microkit
MICROKIT_CONFIG ?= debug
BUILD_DIR ?= build
SERIAL_CONFIG_INCLUDE ?= include/serial_config

ifeq ($(strip $(MICROKIT_BOARD)), odroidc4)
	DRIVER_DIR := meson
	CPU := cortex-a55
else ifeq ($(strip $(MICROKIT_BOARD)), qemu_arm_virt)
	DRIVER_DIR := arm
	CPU := cortex-a53
else ifeq ($(strip $(MICROKIT_BOARD)), maaxboard)
	DRIVER_DIR := imx
	CPU := cortex-a53
else
$(error Unsupported MICROKIT_BOARD given)
endif

SDDF=$(abspath ../..)
UTIL=$(SDDF)/util
SERIAL_COMPONENTS=$(SDDF)/serial/components
UART_DRIVER=$(SDDF)/drivers/serial/$(DRIVER_DIR)
BOARD_DIR := $(MICROKIT_SDK)/board/$(MICROKIT_BOARD)/$(MICROKIT_CONFIG)
SYSTEM_FILE := board/$(MICROKIT_BOARD)/serial.system

IMAGES := uart.elf serial_server.elf virt_tx.elf virt_rx.elf
CFLAGS := -mcpu=$(CPU) -mstrict-align -ffreestanding -g3 -O3 -Wall -Wno-unused-function -Werror
LDFLAGS := -L$(BOARD_DIR)/lib -L$(SDDF)/lib
LIBS := -lmicrokit -Tmicrokit.ld -lc

IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt

CFLAGS += -I$(BOARD_DIR)/include \
	-Iinclude	\
	-I$(SERIAL_CONFIG_INCLUDE) \
	-I$(UART_DRIVER)/include \
	-I$(SDDF)/include \

UART_OBJS := uart.o printf.o putchar_debug.o
SERIAL_SERVER_OBJS := serial_server.o
VIRT_TX_OBJS := virt_tx.o
VIRT_RX_OBJS := virt_rx.o

all: directories $(IMAGE_FILE)

$(BUILD_DIR)/%.o: $(SERIAL_COMPONENTS)/%.c Makefile
	$(CC) -c $(CFLAGS) -DSERIAL_TRANSFER_WITH_COLOUR $< -o $@

$(BUILD_DIR)/%.o: $(SERIAL_QUEUE)/%.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: $(UART_DRIVER)/%.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/printf.o: $(UTIL)/printf.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/putchar_debug.o: $(UTIL)/putchar_debug.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/serial_server.o: serial_server.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/virt_tx.elf: $(addprefix $(BUILD_DIR)/, $(VIRT_TX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/virt_rx.elf: $(addprefix $(BUILD_DIR)/, $(VIRT_RX_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/uart.elf: $(addprefix $(BUILD_DIR)/, $(UART_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/serial_server.elf: $(addprefix $(BUILD_DIR)/, $(SERIAL_SERVER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE) $(REPORT_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) $(SYSTEM_FILE)
	$(MICROKIT_TOOL) $(SYSTEM_FILE) --search-path $(BUILD_DIR) --board $(MICROKIT_BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

.PHONY: all depend compile clean

qemu: all
	$(QEMU) -machine virt,virtualization=on -cpu cortex-a53 -serial mon:stdio -device loader,file=$(IMAGE_FILE),addr=0x70000000,cpu-num=0 -m size=2G -nographic

directories:
	mkdir -p $(BUILD_DIR)

clean:
	rm -f *.o *.elf .depend*
	find . -name \*.o |xargs --no-run-if-empty rm

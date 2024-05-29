#
# Copyright 2022, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#

ifeq ($(strip $(BUILD_DIR)),)
$(error BUILD_DIR must be specified)
endif

ifeq ($(strip $(MICROKIT_SDK)),)
$(error MICROKIT_SDK must be specified)
endif

ifeq ($(strip $(MICROKIT_BOARD)),)
$(error MICROKIT_BOARD must be specified)
endif

ifeq ($(strip $(MICROKIT_CONFIG)),)
$(error MICROKIT_CONFIG must be specified)
endif

ifeq ($(strip $(CAKE_COMPILER)),)
$(error CAKE_COMPILER must be specified)
endif

TOOLCHAIN := aarch64-none-elf
CPU := cortex-a53

CC := $(TOOLCHAIN)-gcc
LD := $(TOOLCHAIN)-ld
AS := $(TOOLCHAIN)-as
MICROKIT_TOOL ?= $(MICROKIT_SDK)/bin/microkit

RINGBUFFERDIR=libsharedringbuffer

BOARD_DIR := $(MICROKIT_SDK)/board/$(MICROKIT_BOARD)/$(MICROKIT_CONFIG)

IMAGES := serial.elf serial_server.elf
CFLAGS := -mcpu=$(CPU) -mstrict-align -ffreestanding -g3 -O3 -Wall  -Wno-unused-function
LDFLAGS := -L$(BOARD_DIR)/lib -Llib
LIBS := -lc -lmicrokit -Tmicrokit.ld

IMAGE_FILE = $(BUILD_DIR)/loader.img
REPORT_FILE = $(BUILD_DIR)/report.txt

CFLAGS += -I$(BOARD_DIR)/include \
	-Iinclude	\
	-I$(RINGBUFFERDIR)/include \

SERIAL_OBJS := serial.o libsharedringbuffer/shared_ringbuffer.o serial_ffi.o debug.o
SERIAL_SERVER_OBJS := serial_server.o libsharedringbuffer/shared_ringbuffer.o
all: directories $(IMAGE_FILE)

$(BUILD_DIR)/serial.o: $(BUILD_DIR)/serial.S Makefile
	$(CC) -c -mcpu=$(CPU) $< -o $@

# Define the list of .🥞 files
PANCAKE_FILES = serial.🥞 serial_helper.🥞 util.🥞 libsharedringbuffer/shared_ringbuffer.🥞 libsharedringbuffer/ringbuff_helper.🥞

$(BUILD_DIR)/serial.S: $(PANCAKE_FILES) Makefile
	cat $(PANCAKE_FILES) | $(CAKE_COMPILER) --target=arm8 --pancake --main_return=true > $@

$(BUILD_DIR)/%.o: %.c Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o: debug.c debug.h Makefile
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/serial.elf: $(addprefix $(BUILD_DIR)/, $(SERIAL_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(BUILD_DIR)/serial_server.elf: $(addprefix $(BUILD_DIR)/, $(SERIAL_SERVER_OBJS))
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

$(IMAGE_FILE) $(REPORT_FILE): $(addprefix $(BUILD_DIR)/, $(IMAGES)) serial.system
	$(MICROKIT_TOOL) serial.system --search-path $(BUILD_DIR) --board $(MICROKIT_BOARD) --config $(MICROKIT_CONFIG) -o $(IMAGE_FILE) -r $(REPORT_FILE)

.PHONY: all depend compile clean

%.o:
	$(CC) $(CFLAGS) -c $(@:.o=.c) -o $@

#Make the Directories
directories:
	$(info $(shell mkdir -p $(BUILD_DIR)/libsharedringbuffer))	\

clean:
	rm -f *.o *.elf .depend*
	find . -name \*.o |xargs --no-run-if-empty rm
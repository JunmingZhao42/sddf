#
# Copyright 2024, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Include this snippet in your project Makefile to build
# the IMX8 NIC driver
#
# NOTES
#  Generates eth_driver.elf
#  Expects System Description File to set eth_regs to the address of
#  the registers
#  Expects libsddf_util_debug.a to be in LIBS

ETHERNET_DRIVER_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
NET_QUEUE_DIR := ${SDDF}/include/sddf/network

CHECK_NETDRV_FLAGS_MD5:=.netdrv_cflags-$(shell echo -- ${CFLAGS} ${CFLAGS_network} | shasum | sed 's/ *-//')
${CHECK_NETDRV_FLAGS_MD5}:
	-rm -f .netdrv_cflags-*
	touch $@

NETWORK_DRIVER_PNK = ${UTIL}/util.🥞 \
	${NET_QUEUE_DIR}/net_queue.🥞 \
	${ETHERNET_DRIVER_DIR}/hw_helper.🥞 \
	${ETHERNET_DRIVER_DIR}/ethernet.🥞

eth_pnk.o: eth_pnk.S
	$(CC) -c -mcpu=$(CPU) $< -o $@

eth_pnk.S: $(NETWORK_DRIVER_PNK)
	cat $(NETWORK_DRIVER_PNK) | cpp -P | $(CAKE_COMPILER) --target=arm8 --pancake --main_return=true > $@

imx/ethernet.o: ${ETHERNET_DRIVER_DIR}/ethernet.c ${CHECK_NETDRV_FLAGS_MD5}
	mkdir -p imx
	${CC} -c ${CFLAGS} ${CFLAGS_network} -I ${ETHERNET_DRIVER} -o $@ $<

eth_driver.elf: eth_pnk.o imx/ethernet.o pancake_ffi.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

imx/ethernet.o: ${ETHERNET_DRIVER_DIR}/ethernet.c ${CHECK_NETDRV_FLAGS_MD5}
	mkdir -p imx
	${CC} -c ${CFLAGS} ${CFLAGS_network} -I ${ETHERNET_DRIVER} -o $@ $<

-include imx/ethernet.d
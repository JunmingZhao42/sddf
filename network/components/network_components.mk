#
# Copyright 2023, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#
# This Makefile snippet builds the network components
# (for example, simple RX and TX virtualisers)
# it should be included into your project Makefile
#
# NOTES:
# Generates network_virt_rx.elf network_virt_tx.elf arp.elf copy.elf
# Relies on the variable NUM_NETWORK_CLIENTS to be a flag for
# the C compiler to configure the virtualisers
# Requires ${SDDF}/util/util.mk to build the utility library for debug output

ifeq ($(strip $(NUM_NETWORK_CLIENTS)),)
$(error Specify the number of clients for the network virtualisers.  Expect -DNUM_NETWORK_CLIENTS=3 or similar)
endif


NETWORK_COMPONENTS_DIR := $(abspath $(dir $(lastword ${MAKEFILE_LIST})))
NETWORK_IMAGES:= network_virt_rx.elf network_virt_tx.elf arp.elf copy.elf
network/components/%.o: ${SDDF}/network/components/%.c
	${CC} ${CFLAGS} -c -o $@ $<

NETWORK_COMPONENT_OBJ := $(addprefix network/components/, copy.o arp.o network_virt_tx.o network_virt_rx.o)

CFLAGS_network += ${NUM_NETWORK_CLIENTS} -I${SDDF}/include/sddf/util

CHECK_NETWORK_FLAGS_MD5:=.network_cflags-$(shell echo -- ${CFLAGS} ${CFLAGS_network} | shasum | sed 's/ *-//')

${CHECK_NETWORK_FLAGS_MD5}:
	-rm -f .network_cflags-*
	touch $@

#vpath %.c ${SDDF}/network/components


${NETWORK_IMAGES): LIBS := libsddf_util_debug.a ${LIBS}

${NETWORK_COMPONENT_OBJ}: |network/components
${NETWORK_COMPONENT_OBJ}: ${CHECK_NETWORK_FLAGS_MD5}
${NETWORK_COMPONENT_OBJ}: CFLAGS+=${CFLAGS_network}

network/components/network_virt_rx.o: ${SDDF}/network/components/virt_rx.c
	${CC} ${CFLAGS} -c -o $@ $<

network/components/network_virt_tx.o: ${SDDF}/network/components/virt_tx.c
	${CC} ${CFLAGS} -c -o $@ $<

COPY_PNK = ${UTIL}/util.🥞 \
	${NETWORK_QUEUE_INCLUDE}/queue_helper.🥞 \
	${NETWORK_QUEUE_INCLUDE}/queue.🥞 \
	${SDDF}/network/components/copy.🥞

copy_pnk.o: copy_pnk.S
	$(CC) -c -mcpu=$(CPU) $< -o $@

copy_pnk.S: $(COPY_PNK)
	cat $(COPY_PNK) | cpp -P | $(CAKE_COMPILER) --target=arm8 --pancake --main_return=true > $@

copy.elf: copy_pnk.o network/components/copy.o pancake_ffi.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

NETWORK_VIRT_RX_PNK = ${UTIL}/util.🥞 \
	${NETWORK_QUEUE_INCLUDE}/queue_helper.🥞 \
	${NETWORK_QUEUE_INCLUDE}/queue.🥞 \
	${SDDF}/network/components/virt_rx.🥞

network_virt_rx_pnk.o: network_virt_rx_pnk.S
	$(CC) -c -mcpu=$(CPU) $< -o $@

network_virt_rx_pnk.S: $(NETWORK_VIRT_RX_PNK)
	cat $(NETWORK_VIRT_RX_PNK) | cpp -P | $(CAKE_COMPILER) --target=arm8 --pancake --main_return=true > $@

network_virt_rx.elf: network_virt_rx_pnk.o network/components/network_virt_rx.o pancake_ffi.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

NETWORK_VIRT_TX_PNK = ${UTIL}/util.🥞 \
	${NETWORK_QUEUE_INCLUDE}/queue_helper.🥞 \
	${NETWORK_QUEUE_INCLUDE}/queue.🥞 \
	${SDDF}/network/components/virt_tx.🥞

network_virt_tx_pnk.o: network_virt_tx_pnk.S
	$(CC) -c -mcpu=$(CPU) $< -o $@

network_virt_tx_pnk.S: $(NETWORK_VIRT_TX_PNK)
	cat $(NETWORK_VIRT_TX_PNK) | cpp -P | $(CAKE_COMPILER) --target=arm8 --pancake --main_return=true > $@

network_virt_tx.elf: network_virt_tx_pnk.o network/components/network_virt_tx.o pancake_ffi.o
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

clean::
	rm -f network_virt_[rt]x.[od] copy.[od] arp.[od]

clobber::
	rm -f ${IMAGES}

network/components:
	mkdir -p $@

-include ${NETWORK_COMPONENTS_OBJS:.o=.d}

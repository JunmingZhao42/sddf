#
# Copyright 2023, UNSW
#
# SPDX-License-Identifier: BSD-2-Clause
#
# This Makefile snippet builds the serial RX and TX virtualisers
# it should be included into your project Makefile
#
# NOTES:
#  Generates serial_virt_rx.elf serial_virt_tx.elf
#

SERIAL_IMAGES:= serial_virt_rx.elf serial_virt_tx.elf

CFLAGS_serial := -I ${SDDF}/include

CHECK_SERIAL_FLAGS_MD5:=.serial_cflags-$(shell echo -- ${CFLAGS} ${CFLAGS_serial} | shasum | sed 's/ *-//')

${CHECK_SERIAL_FLAGS_MD5}:
	-rm -f .serial_cflags-*
	touch $@

# VIRT_RX_PNK = ${UTIL}/util.🥞 \
# 			${SERIAL_QUEUE_INCLUDE}/queue_helper.🥞 \
# 			${SERIAL_QUEUE_INCLUDE}/queue.🥞 \
# 			${SDDF}/serial/components/virt_rx.🥞

# VIRT_TX_PNK = ${UTIL}/util.🥞 \
# 			${SERIAL_QUEUE_INCLUDE}/queue_helper.🥞 \
# 			${SERIAL_QUEUE_INCLUDE}/queue.🥞 \
# 			${SDDF}/serial/components/virt_tx.🥞

serial_virt_%.elf: virt_%.o libsddf_util_debug.a
	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

virt_tx.o virt_rx.o: ${CHECK_SERIAL_FLAGS_MD5}
virt_%.o: ${SDDF}/serial/components/virt_%.c
	${CC} ${CFLAGS} ${CFLAGS_serial} -o $@ -c $<

# serial_%_virt.elf: virt_%_pnk.o virt_%.o pancake_ffi.o
# 	$(LD) $(LDFLAGS) $^ $(LIBS) -o $@

# virt_%_pnk.o: virt_%_pnk.S
# 	$(CC) -c -mcpu=$(CPU) $< -o $@

# virt_rx_pnk.S: $(VIRT_RX_PNK)
# 	cat $(VIRT_RX_PNK) | cpp -P | $(CAKE_COMPILER) --target=arm8 --pancake --main_return=true > $@

# virt_tx_pnk.S: $(VIRT_TX_PNK)
# 	cat $(VIRT_TX_PNK) | cpp -P | $(CAKE_COMPILER) --target=arm8 --pancake --main_return=true > $@

# virt_tx.o virt_rx.o: ${CHECK_SERIAL_FLAGS_MD5}
# virt_%.o: ${SDDF}/serial/components/virt_%.c
# 	${CC} ${CFLAGS} ${CFLAGS_serial} -o $@ -c $<

clean::
	rm -f serial_virt_[rt]x.[od] .serial_cflags-*

clobber::
	rm -f ${SERIAL_IMAGES}


-include virt_rx.d
-include virt_tx.d

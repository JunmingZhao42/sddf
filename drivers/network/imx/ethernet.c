/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <microkit.h>
#include <sddf/network/queue.h>
#include <sddf/util/util.h>
#include <sddf/util/fence.h>
#include <sddf/util/printf.h>
#include <ethernet_config.h>

#include "ethernet.h"

#define IRQ_CH 0
#define TX_CH  1
#define RX_CH  2

uintptr_t eth_regs;
uintptr_t hw_ring_buffer_vaddr;
uintptr_t hw_ring_buffer_paddr;

uintptr_t rx_free;
uintptr_t rx_active;
uintptr_t tx_free;
uintptr_t tx_active;

#define RX_COUNT 256
#define TX_COUNT 256
#define MAX_COUNT MAX(RX_COUNT, TX_COUNT)

_Static_assert((RX_COUNT + TX_COUNT) * 2 * NET_BUFFER_SIZE <= NET_DATA_REGION_SIZE,
               "Expect rx+tx buffers to fit in single 2MB page");

/* HW ring descriptor (shared with device) */
// NOTE: this is just one word
struct descriptor {
    uint16_t len;
    uint16_t stat;
    uint32_t addr;
};

/* HW ring buffer data type */
typedef struct {
    uint64_t tail; /* index to insert at */
    uint64_t head; /* index to remove from */
    net_buff_desc_t descr_mdata[MAX_COUNT]; /* associated meta data array */
    volatile struct descriptor *descr; /* buffer descripter array */
} hw_ring_t;

hw_ring_t *hw_ring_rx; /* Rx NIC ring */
hw_ring_t *hw_ring_tx; /* Tx NIC ring */

net_queue_handle_t *rx_queue;
net_queue_handle_t *tx_queue;

#define MAX_PACKET_SIZE     1536

volatile struct enet_regs *eth;
uint64_t *irq_mask;

static char cml_memory[1024*11];
extern void *cml_heap;
extern void *cml_stack;
extern void *cml_stackend;

extern void cml_main(void);
extern void pnk_notified(microkit_channel ch);

void cml_exit(int arg) {
    microkit_dbg_puts("ERROR! We should not be getting here\n");
}

void cml_err(int arg) {
    if (arg == 3) {
        microkit_dbg_puts("Memory not ready for entry. You may have not run the init code yet, or be trying to enter during an FFI call.\n");
    }
  cml_exit(arg);
}

/* Need to come up with a replacement for this clear cache function. Might be worth testing just flushing the entire l1 cache, but might cause issues with returning to this file*/
void cml_clear() {
    microkit_dbg_puts("Trying to clear cache\n");
}

void init_pancake_mem() {
    unsigned long cml_heap_sz = 1024*9;
    unsigned long cml_stack_sz = 1024;
    cml_heap = cml_memory;
    cml_stack = cml_heap + cml_heap_sz;
    cml_stackend = cml_stack + cml_stack_sz;
}

void init_pancake_data() {
    uintptr_t *heap = (uintptr_t *)cml_heap;
    heap[5] = eth_regs;
    heap[6] = hw_ring_buffer_vaddr;
    heap[7] = hw_ring_buffer_paddr;
    heap[8] = rx_free;
    heap[9] = rx_active;
    heap[10] = tx_free;
    heap[11] = tx_active;
    irq_mask = (uint64_t *) &heap[12];
    *irq_mask = IRQ_MASK;
    rx_queue = (net_queue_handle_t *) &heap[13];
    tx_queue = (net_queue_handle_t *) &heap[16];
    hw_ring_rx = (hw_ring_t *) &heap[19];
    int offset = sizeof(hw_ring_t);
    hw_ring_tx = (hw_ring_t *) ((char *) &heap[19] + offset);
}

static void eth_setup(void)
{
    eth = (void *)eth_regs;
    uint32_t l = eth->palr;
    uint32_t h = eth->paur;

    /* Set up HW rings */
    hw_ring_rx->descr = (volatile struct descriptor *) hw_ring_buffer_vaddr;
    hw_ring_tx->descr = (volatile struct descriptor *)(hw_ring_buffer_vaddr + (sizeof(struct descriptor) * RX_COUNT));

    /* Perform reset */
    eth->ecr = ECR_RESET;
    while (eth->ecr & ECR_RESET);
    eth->ecr |= ECR_DBSWP;

    /* Clear and mask interrupts */
    eth->eimr = 0x00000000;
    eth->eir  = 0xffffffff;

    /* set MDIO freq */
    eth->mscr = 24 << 1;

    /* Disable */
    eth->mibc |= MIBC_DIS;
    while (!(eth->mibc & MIBC_IDLE));
    /* Clear */
    eth->mibc |= MIBC_CLEAR;
    while (!(eth->mibc & MIBC_IDLE));
    /* Restart */
    eth->mibc &= ~MIBC_CLEAR;
    eth->mibc &= ~MIBC_DIS;

    /* Descriptor group and individual hash tables - Not changed on reset */
    eth->iaur = 0;
    eth->ialr = 0;
    eth->gaur = 0;
    eth->galr = 0;

    /* Mac address needs setting again. */
    if (eth->palr == 0) {
        eth->palr = l;
        eth->paur = h;
    }

    eth->opd = PAUSE_OPCODE_FIELD;

    /* coalesce transmit IRQs to batches of 128 */
    eth->txic0 = ICEN | ICFT(128) | 0xFF;
    eth->tipg = TIPG;
    /* Transmit FIFO Watermark register - store and forward */
    eth->tfwr = STRFWD;
    /* clear rx store and forward. This must be done for hardware csums*/
    eth->rsfl = 0;
    /* Do not forward frames with errors + check the csum */
    eth->racc = RACC_LINEDIS | RACC_IPDIS | RACC_PRODIS;
    /* Add the checksum for known IP protocols */
    eth->tacc = TACC_PROCHK | TACC_IPCHK;

    /* Set RDSR */
    eth->rdsr = hw_ring_buffer_paddr;
    eth->tdsr = hw_ring_buffer_paddr + (sizeof(struct descriptor) * RX_COUNT);

    /* Size of max eth packet size */
    eth->mrbr = MAX_PACKET_SIZE;

    eth->rcr = RCR_MAX_FL(1518) | RCR_RGMII_EN | RCR_MII_MODE | RCR_PROMISCUOUS;
    eth->tcr = TCR_FDEN;

    /* set speed */
    eth->ecr |= ECR_SPEED;

    /* Set Enable  in ECR */
    eth->ecr |= ECR_ETHEREN;

    eth->rdar = RDAR_RDAR;

    /* enable events */
    eth->eir = eth->eir;
    eth->eimr = IRQ_MASK;
}

void init(void)
{
    init_pancake_mem();
    init_pancake_data();
    eth_setup();

    net_queue_init(rx_queue, (net_queue_t *)rx_free, (net_queue_t *)rx_active, NET_RX_QUEUE_SIZE_DRIV);
    net_queue_init(tx_queue, (net_queue_t *)tx_free, (net_queue_t *)tx_active, NET_TX_QUEUE_SIZE_DRIV);

    cml_main();
}

void notified(microkit_channel ch)
{
    pnk_notified(ch);
}
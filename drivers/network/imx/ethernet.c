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
#include <sddf/network/hw_ring.h>

#define IRQ_CH 0
#define TX_CH  1
#define RX_CH  2


_Static_assert((RX_COUNT + TX_COUNT) * 2 * NET_BUFFER_SIZE <= NET_DATA_REGION_SIZE,
               "Expect rx+tx buffers to fit in single 2MB page");

uintptr_t eth_regs;
uintptr_t hw_ring_buffer_vaddr;
uintptr_t hw_ring_buffer_paddr;

uintptr_t rx_free;
uintptr_t rx_active;
uintptr_t tx_free;
uintptr_t tx_active;

/* For pancake */
#define CML_HEAP_SZ 1024*2
#define CML_STACK_SZ 1024*2
#define CML_MEM_SZ CML_HEAP_SZ + CML_STACK_SZ

static char cml_memory[CML_MEM_SZ];

extern void cml_main(void);
extern void pnk_rx_provide(void);
extern void pnk_rx_return(void);
extern void pnk_tx_provide(void);
extern void pnk_tx_return(void);
extern void pnk_handle_irq(void);

extern void *cml_heap;
extern void *cml_stack;
extern void *cml_stackend;

void init_pnk_mem() {
    /* Define region ranges */
    cml_heap = cml_memory;
    cml_stack = cml_heap + CML_HEAP_SZ;
    cml_stackend = cml_stack + CML_STACK_SZ;
}

hw_ring_t rx; /* Rx NIC ring */
hw_ring_t tx; /* Tx NIC ring */

net_queue_handle_t rx_queue;
net_queue_handle_t tx_queue;

#define MAX_PACKET_SIZE     1536

volatile struct enet_regs *eth;
/* Changed irq_mask from 32 to 64 bits */
uint64_t irq_mask = IRQ_MASK;

void init_pnk_data() {
    /* Reserve heap 0-4th words */
    /* Note: will require shared memory access */
    uintptr_t *heap = (uintptr_t *) cml_heap;
    heap[5] = (uintptr_t) eth;  /* Regs */
    heap[6] = (uintptr_t) &irq_mask;
    heap[7] = (uintptr_t) &rx_queue;
    heap[8] = (uintptr_t) &tx_queue;
    heap[9] = (uintptr_t) &rx;
    heap[10] = (uintptr_t) &tx;
    /* reserve heap[42] for transporting hardware register value */
}

void cml_exit(int arg) {
    microkit_dbg_puts("ERROR! We should not be getting here\n");
}

void cml_err(int arg) {
    if (arg == 3) {
        microkit_dbg_puts("Memory not ready for entry. You may have not run the init code yet, or be trying to enter during an FFI call.\n");
    }
  cml_exit(arg);
}

// Need to come up with a replacement for this clear cache function. Might be worth testing just flushing the entire l1 cache, but might cause issues with returning to this file
void cml_clear() {
    microkit_dbg_puts("Trying to clear cache\n");
}

static void eth_setup(void)
{
    eth = (void *)eth_regs;
    uint32_t l = eth->palr;
    uint32_t h = eth->paur;

    /* Set up HW rings */
    rx.descr = (volatile struct descriptor *)hw_ring_buffer_vaddr;
    tx.descr = (volatile struct descriptor *)(hw_ring_buffer_vaddr + (sizeof(struct descriptor) * RX_COUNT));

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
    eth_setup();
    init_pnk_mem();
    init_pnk_data();

    /* Implementations in pancake also exist */
    net_queue_init(&rx_queue, (net_queue_t *)rx_free, (net_queue_t *)rx_active, NET_RX_QUEUE_SIZE_DRIV);
    net_queue_init(&tx_queue, (net_queue_t *)tx_free, (net_queue_t *)tx_active, NET_TX_QUEUE_SIZE_DRIV);

    cml_main();

    pnk_rx_provide();
    pnk_tx_provide();
}

/* notified calls corresponding pancake handlers */
void notified(microkit_channel ch)
{
    switch (ch) {
    case IRQ_CH:
        pnk_handle_irq();
        /*
         * Delay calling into the kernel to ack the IRQ until the next loop
         * in the microkit event handler loop.
         */
        microkit_irq_ack_delayed(ch);
        break;
    case RX_CH:
        pnk_rx_provide();
        break;
    case TX_CH:
        pnk_tx_provide();
        break;
    default:
        sddf_dprintf("ETH|LOG: received notification on unexpected channel: %u\n", ch);
        break;
    }
}
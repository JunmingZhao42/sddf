#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sddf/util/util.h>
#include <sddf/network/queue.h>

#define MAX_COUNT MAX(RX_COUNT, TX_COUNT)

/* Hardware constructs and helpers; TODO: find somewhere better to put this */
/* HW ring descriptor (shared with device) */
struct descriptor {
    uint16_t len;
    uint16_t stat;
    uint32_t addr;
};

/* HW ring buffer data type */
typedef struct {
    unsigned int tail; /* index to insert at */
    unsigned int head; /* index to remove from */
    net_buff_desc_t descr_mdata[MAX_COUNT]; /* associated meta data array */
    volatile struct descriptor *descr; /* buffer descripter array */
} hw_ring_t;

static inline bool hw_ring_full(hw_ring_t *ring, size_t ring_size)
{
    return !((ring->tail - ring->head + 1) % ring_size);
}

static inline bool hw_ring_empty(hw_ring_t *ring, size_t ring_size)
{
    return !((ring->tail - ring->head) % ring_size);
}

static void update_ring_slot(hw_ring_t *ring, unsigned int idx, uintptr_t phys,
                             uint16_t len, uint16_t stat)
{
    volatile struct descriptor *d = &(ring->descr[idx]);
    d->addr = phys;
    d->len = len;

    /* Ensure all writes to the descriptor complete, before we set the flags
     * that makes hardware aware of this slot.
     */
    __sync_synchronize();
    d->stat = stat;
}
/*
 * Copyright 2022, UNSW
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "shared_ringbuffer.h"

void ring_init(ring_handle_t *ring, ring_buffer_t *avail, ring_buffer_t *used, notify_fn notify, int buffer_init)
{
    ring->used_ring = used;
    ring->avail_ring = avail;
    ring->notify = notify;

    if (buffer_init) {
        ring->used_ring->write_idx = 0;
        ring->used_ring->read_idx = 0;
        ring->avail_ring->write_idx = 0;
        ring->avail_ring->read_idx = 0;
    }
}

void ffiTHREAD_MEMORY_RELEASE(unsigned char *c, long clen, unsigned char *a, long alen) {
    // TODO: remove this
    if (clen != 42 || alen != 42 || c == NULL || a == NULL) {
        // Just to make sure that we've got the corresponding args
        microkit_dbg_puts("Invalid arguments to ffiTHREAD_MEMORY_RELEASE\n");
        return;
    }
    THREAD_MEMORY_RELEASE();
}

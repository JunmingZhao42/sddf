#include <stdio.h>
#include <stdlib.h>
#include <microkit.h>
#include <sddf/util/fence.h>
#include <sddf/util/util.h>
#include <sddf/network/hw_ring.h>
#include <sddf/network/queue.h>
#include <sddf/util/cache.h>

void print_address(void* addr) {
    char buf[16];
    int i;
    unsigned long int_addr = (unsigned long)addr;

    // Convert the address to a string representation
    for (i = 0; i < 16; i++) {
        unsigned char nibble = (int_addr >> (4 * (15 - i))) & 0xF;
        if (nibble < 10) {
            buf[i] = '0' + nibble;
        } else {
            buf[i] = 'A' + (nibble - 10);
        }
    }

    // Print the "0x" prefix
    microkit_dbg_putc('0');
    microkit_dbg_putc('x');

    // Print the address string
    for (i = 0; i < 16; i++) {
        microkit_dbg_putc(buf[i]);
    }
    microkit_dbg_putc('\n');
}

void print_int(int num) {
    if (num < 0) {
        microkit_dbg_putc('-');
        num = -num;
    }

    if (num == 0) {
        microkit_dbg_putc('0');
        return;
    }

    char buf[10];
    int i = 0;

    while (num != 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }

    while (i > 0) {
        microkit_dbg_putc(buf[--i]);
    }
}

void ffiprint_int(unsigned char *c, long clen, unsigned char *a, long alen) {
    microkit_dbg_puts("FFI print int:\n");
    print_int(clen);
    microkit_dbg_putc(',');
    print_int(alen);
    microkit_dbg_putc('\n');
}

void ffiprint_char(unsigned char *c, long clen, unsigned char *a, long alen) {
    microkit_dbg_puts("FFI print char:\n");
    microkit_dbg_putc(((char) clen) + 48);
    microkit_dbg_putc(',');
    microkit_dbg_putc(((char) alen) + 48);
    microkit_dbg_putc('\n');
}

void ffiprint_string(unsigned char *c, long clen, unsigned char *a, long alen) {
    for (int i = 0; i < clen; i++) {
        microkit_dbg_putc(c[i]);
    }
    microkit_dbg_putc('\n');
}

void ffiprint_address(unsigned char *c, long clen, unsigned char *a, long alen) {
    microkit_dbg_puts("FFI print address:\n");
    microkit_dbg_putc(((char) clen) + 48);
    microkit_dbg_putc(',');
    print_address((void *) c);
    microkit_dbg_putc(((char) alen) + 48);
    microkit_dbg_putc(',');
    print_address((void *) a);
    microkit_dbg_putc('\n');
}

/*
    * helper function to copy half word from *c to *a
    * TODO: should be replaced later by pancake
    * c: memory address
    * a: memory address
*/
void ffitransfer_hw(unsigned char *c, long clen, unsigned char *a, long alen) {
    uint32_t value = *(uint32_t *) c;
    *(uint32_t *) a = value;
}

void ffifuncall(unsigned char *c, long clen, unsigned char *a, long alen) {
    if (clen != 1) {
        microkit_dbg_puts("ld_hw: There are no arguments supplied when args are expected");
        c[0] = 0;
        return;
    }
    // c is the address of the function to call
    void (*func)(void) = (void (*)(void))c;
    func();
}

void ffiTHREAD_MEMORY_RELEASE(unsigned char *c, long clen, unsigned char *a, long alen) {
    // TODO: remove this
    if (clen != 42 || alen != 42 || c != NULL || a != NULL) {
        // Just to make sure that we've got the corresponding args
        microkit_dbg_puts("Invalid arguments to ffiTHREAD_MEMORY_RELEASE\n");
        return;
    }
    THREAD_MEMORY_RELEASE();
}

void ffiassert(unsigned char *c, long clen, unsigned char *a, long alen) {
    // clen is the condition
    assert(clen);
}

void ffimicrokit_notify(unsigned char *c, long clen, unsigned char *a, long alen) {
    // clen is the notification channel
    microkit_notify(clen);
}

void ffimicrokit_irq_ack_delayed(unsigned char *c, long clen, unsigned char *a, long alen) {
    // clen is the notification channel
    microkit_irq_ack_delayed(clen);
}

void ffimicrokit_notify_delayed(unsigned char *c, long clen, unsigned char *a, long alen) {
    // clen is the notification channel
    microkit_notify_delayed(clen);
}

/* Network hardware (NIC) ring operations */
void ffiget_hw_head(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t target_ring = *(hw_ring_t *)c;
    *(unsigned int *)a = target_ring.head;
}

void ffiget_hw_tail(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t target_ring = *(hw_ring_t *)c;
    *(unsigned int *)a = target_ring.tail;
}

void ffiset_hw_head(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t target_ring = *(hw_ring_t *)c;
    target_ring.head = (target_ring.head + 1) % (clen ? TX_COUNT : RX_COUNT);
}

void ffiset_hw_tail(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t target_ring = *(hw_ring_t *)c;
    target_ring.tail = (target_ring.tail + 1) % (clen ? TX_COUNT : RX_COUNT);
}

void ffihw_ring_full(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t target_ring = *(hw_ring_t *)c;
    *(uint64_t *)a = !((target_ring.tail - target_ring.head + 1) % (clen ? TX_COUNT : RX_COUNT));
}

void ffihw_ring_empty(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t target_ring = *(hw_ring_t *)c;
    *(uint64_t *)a = !((target_ring.tail - target_ring.head) % (clen ? TX_COUNT : RX_COUNT));
}

void ffiget_hw_meta(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t *r = (hw_ring_t *)c;
    net_buff_desc_t buffer = r->descr_mdata[clen];
    *(net_buff_desc_t *)a = buffer;
}

void ffiset_hw_meta(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t *r = (hw_ring_t *)c;
    net_buff_desc_t buffer = *(net_buff_desc_t *)a;
    r->descr_mdata[r->tail] = buffer;
}

void ffiset_hw_ring_slot(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t *r = (hw_ring_t *)c;
    uint64_t buffer_offset = *(uint64_t *)a;
    update_ring_slot(r, r->tail, buffer_offset, (uint16_t)clen, (uint16_t)alen);
}

void ffiget_hw_descr_stat(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t *r = (hw_ring_t *)c;
    volatile struct descriptor *d = &(r->descr[clen]);
    *(uint16_t *)a = d->stat;
}

void ffiget_hw_descr_len(unsigned char *c, long clen, unsigned char *a, long alen) {
    hw_ring_t *r = (hw_ring_t *)c;
    volatile struct descriptor *d = &(r->descr[clen]);
    *(uint16_t *)a = d->len;
}
void fficache_clean(unsigned char *c, long clen, unsigned char *a, long alen) {
    // c is the start address
    // a is the end address
    cache_clean((unsigned long) c, (unsigned long) a);
}

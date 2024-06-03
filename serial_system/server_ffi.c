#include "server.h"
#include <string.h>
#include <stdlib.h>

/* This flag is on by default. It catches CakeML's out-of-memory exit codes
 * and prints a helpful message to stderr.
 * Note that this is not specified by the basis library.
 * */
#define STDERR_MEM_EXHAUST
#define CONFIG_ENABLE_BENCHMARKS

/* Ring handle components -
Need to have access to the same ring buffer mechanisms as the driver, so that we can enqueue
buffers to be serviced by the driver.*/

uintptr_t rx_avail;
uintptr_t rx_used;
uintptr_t tx_avail;
uintptr_t tx_used;
uintptr_t shared_dma;

static char cml_memory[1024*1024*2];

extern void cml_main(void);
extern void handle_getchar(void);
extern void *cml_heap;
extern void *cml_stack;
extern void *cml_stackend;

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

void ffimicrokit_notify_getchar(unsigned char *c, long clen, unsigned char *a, long alen) {
    microkit_notify(SERVER_GETCHAR_CHANNEL);
}

void ffimicrokit_notify_print(unsigned char *c, long clen, unsigned char *a, long alen) {
    microkit_notify(SERVER_PRINT_CHANNEL);
}

void init_pancake_mem() {
    unsigned long sz = 1024*1024; // 1 MB unit\n",
    unsigned long cml_heap_sz = sz;    // Default: 1 MB heap\n", (* TODO: parameterise *)
    unsigned long cml_stack_sz = sz;   // Default: 1 MB stack\n", (* TODO: parameterise *)
    cml_heap = cml_memory;
    cml_stack = cml_heap + cml_heap_sz;
    cml_stackend = cml_stack + cml_stack_sz;
}

/*
struct serial_server {
    ring_handle_t rx_ring;
    ring_handle_t tx_ring;
};
*/
void init_pancake_data() {
    uintptr_t *heap = (uintptr_t *) cml_heap;
    heap[0] = 0;
    heap[1] = rx_avail;
    heap[2] = rx_used;
    heap[3] = tx_avail;
    heap[4] = tx_used;
    heap[5] = shared_dma;
}

// Init function required by microkit, initialise serial datastructres for server here
void init(void) {
    init_pancake_mem();
    init_pancake_data();
    microkit_dbg_puts(microkit_name);
    microkit_dbg_puts(": elf PD init function running\n");

    cml_main();
}

// Entry point to pancake
void notified(microkit_channel ch) {
    // TODO: not sure why it ignores the first char input
    if (ch == SERVER_GETCHAR_CHANNEL) {
        handle_getchar();
    }
    return;
}
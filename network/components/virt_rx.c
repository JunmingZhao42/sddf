#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <microkit.h>
#include <sddf/network/queue.h>
#include <sddf/network/constants.h>
#include <sddf/util/fence.h>
#include <sddf/util/util.h>
#include <sddf/util/printf.h>
#include <sddf/util/cache.h>
#include <ethernet_config.h>

/* Notification channels */
#define DRIVER_CH 0
#define CLIENT_CH 1

/* Used to signify that a packet has come in for the broadcast address and does not match with
 * any particular client. */
#define BROADCAST_ID (NUM_NETWORK_CLIENTS + 1)

/* Queue regions */
uintptr_t rx_free_drv;
uintptr_t rx_active_drv;
uintptr_t rx_free_cli0;
uintptr_t rx_active_cli0;
uintptr_t rx_free_cli1;
uintptr_t rx_active_cli1;

/* Buffer data regions */
uintptr_t buffer_data_vaddr;
uintptr_t buffer_data_paddr;

/* In order to handle broadcast packets where the same buffer is given to multiple clients
  * we keep track of a reference count of each buffer and only hand it back to the driver once
  * all clients have returned the buffer. */
uint64_t *buffer_refs;

net_queue_handle_t *rx_queue_drv;
net_queue_handle_t *rx_queue_clients;

uint8_t *mac_addrs;

static char cml_memory[1024*8];

extern void cml_main(void);
extern void pnk_notified(microkit_channel ch);
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

// Need to come up with a replacement for this clear cache function. Might be worth testing just flushing the entire l1 cache, but might cause issues with returning to this file
void cml_clear() {
    microkit_dbg_puts("Trying to clear cache\n");
}

void init_pancake_mem() {
    unsigned long cml_heap_sz = 1024*6;
    unsigned long cml_stack_sz = 1024*2;
    cml_heap = cml_memory;
    cml_stack = cml_heap + cml_heap_sz;
    cml_stackend = cml_stack + cml_stack_sz;
}

void init_pancake_data() {
    uintptr_t *heap = (uintptr_t *) cml_heap;
    heap[5] = rx_free_drv;
    heap[6] = rx_active_drv;
    heap[7] = rx_free_cli0;
    heap[8] = rx_active_cli0;
    heap[9] = rx_free_cli1;
    heap[10] = rx_active_cli1;
    heap[11] = buffer_data_vaddr;
    heap[12] = buffer_data_paddr;
    // heap[13] = CONFIG_L1_CACHE_LINE_SIZE_BITS;
    // notify_drv = (uint64_t *) &heap[14];
    rx_queue_drv = (net_queue_handle_t *) &heap[15];
    rx_queue_clients = (net_queue_handle_t *) &heap[18];
    int offset = sizeof(net_queue_handle_t) * NUM_NETWORK_CLIENTS;
    mac_addrs = (uint8_t *) ((char *) &heap[18] + offset);
    buffer_refs = (uint64_t *) &heap[128];
}

void init(void)
{
    init_pancake_mem();
    init_pancake_data();

    net_virt_mac_addr_init_sys(microkit_name, (uint8_t *) mac_addrs);

    net_queue_init(rx_queue_drv, (net_queue_t *)rx_free_drv, (net_queue_t *)rx_active_drv, NET_RX_QUEUE_SIZE_DRIV);
    net_virt_queue_init_sys(microkit_name, rx_queue_clients, rx_free_cli0, rx_active_cli0);
    net_buffers_init(rx_queue_drv, buffer_data_paddr);

    if (net_require_signal_free(rx_queue_drv)) {
        net_cancel_signal_free(rx_queue_drv);
        microkit_notify_delayed(DRIVER_CH);
    }

    cml_main();
}

void notified(microkit_channel ch)
{
    pnk_notified(ch);
}

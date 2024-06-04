#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <microkit.h>
#include <sddf/serial/queue.h>
#include <sddf/util/printf.h>
#include <serial_config.h>

#define DRIVER_CH 0
#define CLIENT_OFFSET 1

serial_queue_t *tx_queue_drv;
uintptr_t tx_queue_cli0;

char *tx_data_drv;
uintptr_t tx_data_cli0;

#if SERIAL_WITH_COLOUR

#define MAX_COLOURS 256
#define MAX_COLOURS_LEN 3

#define COLOUR_START_START "\x1b[38;5;"
#define COLOUR_START_START_LEN 7

#define COLOUR_START_END "m"
#define COLOUR_START_END_LEN 1

#define COLOUR_END "\x1b[0m"

char *clients_colours[SERIAL_NUM_CLIENTS];

#endif

serial_queue_handle_t *tx_queue_handle_drv;
serial_queue_handle_t *tx_queue_handle_cli;

typedef struct tx_pending {
    uint64_t queue[SERIAL_NUM_CLIENTS];
    uint64_t clients_pending[SERIAL_NUM_CLIENTS];
    uint64_t head;
    uint64_t tail;
} tx_pending_t;

tx_pending_t *tx_pending;

static char cml_memory[1024*4];

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

/* Need to come up with a replacement for this clear cache function. Might be worth testing just flushing the entire l1 cache, but might cause issues with returning to this file*/
void cml_clear() {
    microkit_dbg_puts("Trying to clear cache\n");
}

void init_pancake_mem() {
    unsigned long sz = 1024*2;
    unsigned long cml_heap_sz = sz;
    unsigned long cml_stack_sz = sz;
    cml_heap = cml_memory;
    cml_stack = cml_heap + cml_heap_sz;
    cml_stackend = cml_stack + cml_stack_sz;
}

void init_pancake_data() {
    uintptr_t* heap = (uintptr_t*) cml_heap;
    heap[5] = (uintptr_t) tx_queue_drv;
    heap[6] = (uintptr_t) tx_queue_cli0;
    heap[7] = (uintptr_t) tx_data_drv;
    heap[8] = (uintptr_t) tx_data_cli0;

    tx_queue_handle_drv = (serial_queue_handle_t *) &heap[10];
    tx_queue_handle_cli = (serial_queue_handle_t *) &heap[13];
    tx_pending = (tx_pending_t *) &heap[13 + SERIAL_NUM_CLIENTS * sizeof(serial_queue_handle_t) / sizeof(uintptr_t)];
}

void init(void)
{
    init_pancake_mem();
    init_pancake_data();
    serial_queue_init(tx_queue_handle_drv, tx_queue_drv, SERIAL_TX_DATA_REGION_SIZE_DRIV, tx_data_drv);
    serial_virt_queue_init_sys(microkit_name, tx_queue_handle_cli, tx_queue_cli0, tx_data_cli0);

#if !SERIAL_TX_ONLY
    /* Print a deterministic string to allow console input to begin */
    sddf_memcpy(tx_queue_handle_drv->data_region, SERIAL_CONSOLE_BEGIN_STRING, SERIAL_CONSOLE_BEGIN_STRING_LEN);
    serial_update_visible_tail(tx_queue_handle_drv, SERIAL_CONSOLE_BEGIN_STRING_LEN);
    microkit_notify(DRIVER_CH);
#endif

#if SERIAL_WITH_COLOUR
    serial_channel_names_init(clients_colours);
    for (uint64_t i = 0; i < SERIAL_NUM_CLIENTS; i++) {
        sddf_dprintf("%s%lu%s%s is client %lu%s\n", COLOUR_START_START, i % MAX_COLOURS, COLOUR_START_END, clients_colours[i], i,
                     COLOUR_END);
    }
#endif
    cml_main();
}

void notified(microkit_channel ch)
{
    pnk_notified(ch);
}

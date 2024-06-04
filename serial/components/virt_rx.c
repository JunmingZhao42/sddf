#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <microkit.h>
#include <sddf/serial/queue.h>
#include <sddf/util/string.h>
#include <sddf/util/printf.h>
#include <serial_config.h>
#include "uart.h"

#define DRIVER_CH 0
#define CLIENT_OFFSET 1

serial_queue_t *rx_queue_drv;
uintptr_t rx_queue_cli0;

char *rx_data_drv;
uintptr_t rx_data_cli0;

serial_queue_handle_t *rx_queue_handle_drv;
serial_queue_handle_t *rx_queue_handle_cli;

#define MAX_CLI_BASE_10 4
typedef enum mode {normal, switched, number} mode_t;

mode_t *current_mode;
uint64_t *current_client;
uint64_t *next_client_index;
char *next_client;

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
    heap[5] = (uintptr_t) rx_queue_drv;
    heap[6] = (uintptr_t) rx_queue_cli0;
    heap[7] = (uintptr_t) rx_data_drv;
    heap[8] = (uintptr_t) rx_data_cli0;
    current_mode = (mode_t *) &heap[9];
    current_client = (uint64_t *) &heap[10];
    next_client_index = (uint64_t *) &heap[11];
    rx_queue_handle_drv = (serial_queue_handle_t *) &heap[12];
    rx_queue_handle_cli = (serial_queue_handle_t *) &heap[15];
    next_client = (char *) &heap[15 + SERIAL_NUM_CLIENTS * sizeof(serial_queue_handle_t) / sizeof(uintptr_t)];
}

void init(void)
{
    init_pancake_mem();
    init_pancake_data();
    serial_queue_init(rx_queue_handle_drv, rx_queue_drv, SERIAL_RX_DATA_REGION_SIZE_DRIV, rx_data_drv);
    serial_virt_queue_init_sys(microkit_name, rx_queue_handle_cli, rx_queue_cli0, rx_data_cli0);
    cml_main();
}

void notified(microkit_channel ch)
{
    pnk_notified(ch);
}
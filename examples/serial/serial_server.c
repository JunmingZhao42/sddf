#include <ctype.h>
#include <microkit.h>
#include <sddf/serial/queue.h>
#include <sddf/util/printf.h>
#include <serial_config.h>

serial_queue_t *rx_queue;
serial_queue_t *tx_queue;

char *rx_data;
char *tx_data;

serial_queue_handle_t* rx_queue_handle;
serial_queue_handle_t* tx_queue_handle;

static char cml_memory[1024*4];
extern void *cml_heap;
extern void *cml_stack;
extern void *cml_stackend;

extern void cml_main(void);
extern void notified(microkit_channel ch);

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
    heap[5] = (uintptr_t) rx_data;
    heap[6] = (uintptr_t) tx_data;
    heap[7] = (uintptr_t) rx_queue;
    heap[8] = (uintptr_t) tx_queue;
    rx_queue_handle = (serial_queue_handle_t *) &heap[9];
    tx_queue_handle = (serial_queue_handle_t *) &heap[12];
    // local_head = (uint64_t *) &heap[15];
    // local_tail = (uint64_t *) &heap[16];
    heap[17] = 0;
}

void init(void)
{
    init_pancake_mem();
    init_pancake_data();
    serial_cli_queue_init_sys(microkit_name, rx_queue_handle, rx_queue, rx_data, tx_queue_handle, tx_queue, tx_data);
    // serial_putchar_init(TX_CH, tx_queue_handle);
    cml_main();
    // TODO
    // sddf_printf("Hello world! I am %s.\r\nPlease give me character!\r\n", microkit_name);
}

uint16_t char_count;


#include "server.h"
#include "serial.h"
#include "shared_ringbuffer.h"
#include <string.h>
#include <stdlib.h>

/* Ring handle components -
Need to have access to the same ring buffer mechanisms as the driver, so that we can enqueue
buffers to be serviced by the driver.*/

uintptr_t rx_avail;
uintptr_t rx_used;
uintptr_t tx_avail;
uintptr_t tx_used;
uintptr_t shared_dma;

ring_handle_t* rx_ring;
ring_handle_t* tx_ring;

char global_test;

static char cml_memory[1024*1024*2];

extern void cml_main(void);
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
    rx_ring = (ring_handle_t *) &heap[7];
    tx_ring = (ring_handle_t *) &heap[10];
}

// TODO: move to pancake
/*
Return -1 on failure.
*/
int serial_server_printf(char *string) {
    // Get a buffer from the tx ring

    // Address that we will pass to dequeue to store the buffer address
    uintptr_t buffer = 0;
    // Integer to store the length of the buffer
    uint64_t buffer_len = 0;
    void *cookie = 0;

    // Dequeue a buffer from the available ring from the tx buffer
    int ret = dequeue_avail(tx_ring, &buffer, &buffer_len, &cookie);

    if(ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": serial server printf, unable to dequeue from tx ring, tx ring empty\n");
        return -1;
    }

    // Need to copy over the string into the buffer, if it is less than the buffer length
    int print_len = strlen(string) + 1;

    if(print_len > BUFFER_SIZE) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": print string too long for buffer\n");
        return -1;
    }

    // Copy over the string to be printed to the buffer
    memcpy((char *) buffer, string, print_len);

    // We then need to add this buffer to the transmit used ring structure
    bool is_empty = ring_empty(tx_ring->used_ring);

    ret = enqueue_used(tx_ring, buffer, print_len, &cookie);

    if(ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": serial server printf, unable to enqueue to tx used ring\n");
        return -1;
    }

    /*
    First we will check if the transmit used ring is empty. If not empty, then the driver was processing
    the used ring, however it was not finished, potentially running out of budget and being pre-empted.
    Therefore, we can just add the buffer to the used ring, and wait for the driver to resume. However if
    empty, then we can notify the driver to start processing the used ring.
    */

    if(is_empty) {
        // Notify the driver through the printf channel
        microkit_notify(SERVER_PRINT_CHANNEL);
    }

    return 0;
}

// TODO: move to pancake
// Return char on success, -1 on failure
int getchar() {
    // Notify the driver that we want to get a character. In Patrick's design, this increments
    // the chars_for_clients value.
    microkit_notify(SERVER_GETCHAR_CHANNEL);


    /* Now that we have notified the driver, we can attempt to dequeue from the used ring.
    When the driver has processed an interrupt, it will add the inputted character to the used ring.*/

    // Address that we will pass to dequeue to store the buffer address
    uintptr_t buffer = 0;
    // Integer to store the length of the buffer
    uint64_t buffer_len = 0;

    void *cookie = 0;

    while (dequeue_used(rx_ring, &buffer, &buffer_len, &cookie) != 0) {
        /* The ring is currently empty, as there is no character to get.
        We will spin here until we have gotten a character. As the driver is a higher priority than us,
        it should be able to pre-empt this loop
        */
        asm("nop"); /* From Patrick, this is apparently needed to stop the compiler from optimising out the

        as it is currently empty. When compiled in a release version the puts statement will be compiled
        into a nop command.
        */
    }

    // We are only getting one character at a time, so we just need to cast the buffer to an int

    char got_char = *((char *) buffer);

    /* Now that we are finished with the used buffer, we can add it back to the available ring*/

    int ret = enqueue_avail(rx_ring, buffer, buffer_len, NULL);

    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": getchar - unable to enqueue used buffer back into available ring\n");
    }

    return (int) got_char;
}

/* Return 0 on success, -1 on failure.
Basic scanf implementation using the getchar function above. Gets characters until
CTRL+C or CTRL+D or new line.
NOT MEMORY SAFE
*/
int serial_server_scanf(char* buffer) {
    int i = 0;
    int getchar_ret = getchar();

    if (getchar_ret == -1) {
        microkit_dbg_puts("Error getting char\n");
        return -1;
    }


    while(getchar_ret != '\03' && getchar_ret != '\04' && getchar_ret != '\r') {
        ((char *) buffer)[i] = (char) getchar_ret;

        getchar_ret = getchar();

        if (getchar_ret == -1) {
            microkit_dbg_puts("Error getting char\n");
            return -1;
        }

        i++;
    }

    return 0;
}

// Init function required by microkit, initialise serial datastructres for server here
void init(void) {
    init_pancake_mem();
    init_pancake_data();
    cml_main();

    microkit_dbg_puts(microkit_name);
    microkit_dbg_puts(": elf PD init function running\n");

    serial_server_printf("Attempting to use the server printf!\n");
    serial_server_printf("-------HELLO THERE THIS IS SERVER PRINTF-------\n");
    serial_server_printf("Enter char to test getchar\n");
}


void notified(microkit_channel ch) {
    // TODO: not sure why it ignores the first char input
    if (ch == SERVER_GETCHAR_CHANNEL) {
        global_test = getchar();
        serial_server_printf(&global_test);
        if (global_test == '\03' || global_test == '\04' || global_test == '\r') {
            serial_server_printf("\n");
        }
    }
    return;
}
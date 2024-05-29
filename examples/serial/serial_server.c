#include <microkit.h>
#include <sddf/serial/queue.h>
#include <sddf/serial/util.h>

#ifndef SERIAL_SERVER_NUMBER
#error "Need to define SERIAL_SERVER_NUMBER"
#endif

/* Channels to the rest of the serial sub-system. These numbers are
 * follow what is in the system description file.
 */
#define SERIAL_VIRT_TX_CH 0
#define SERIAL_VIRT_RX_CH 1

/* Shared memory regions that hold the serial data and queue structures. */
uintptr_t rx_free;
uintptr_t rx_active;
uintptr_t tx_free;
uintptr_t tx_active;

uintptr_t rx_data;
uintptr_t tx_data;

static serial_queue_handle_t rx_queue;
static serial_queue_handle_t tx_queue;

/*
Return -1 on failure.
*/
int serial_server_printf(char *string)
{
    // Get a buffer from the tx queue

    // Address that we will pass to dequeue to store the buffer address
    uintptr_t buffer = 0;
    // Integer to store the length of the buffer
    unsigned int buffer_len = 0;

    // Dequeue a buffer from the free queue from the tx buffer
    int ret = serial_dequeue_free(&tx_queue, &buffer, &buffer_len);

    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": serial server printf, unable to dequeue from tx queue, tx queue empty\n");
        return -1;
    }

    // Need to copy over the string into the buffer, if it is less than the buffer length
    int print_len = strlen(string) + 1;

    if (print_len > BUFFER_SIZE) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": print string too long for buffer\n");
        return -1;
    }

    // Copy over the string to be printed to the buffer

    memcpy((char *) buffer, string, print_len);

    // We then need to add this buffer to the transmit active queue structure

    bool is_empty = serial_queue_empty(tx_queue.active);

    ret = serial_enqueue_active(&tx_queue, buffer, print_len);

    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": serial server printf, unable to enqueue to tx active queue\n");
        return -1;
    }

    /*
    First we will check if the transmit active queue is empty. If not empty, then the driver was processing
    the active queue, however it was not finished, potentially running out of budget and being pre-empted.
    Therefore, we can just add the buffer to the active queue, and wait for the driver to resume. However if
    empty, then we can notify the driver to start processing the active queue.
    */

    if (is_empty) {
        // Notify the driver through the TX channel
        microkit_notify(SERIAL_VIRT_TX_CH);
    }

    return 0;
}

// Return char on success, -1 on failure
int getchar()
{

    // Notify the driver that we want to get a character. In Patrick's design, this increments
    // the chars_for_clients value.
    microkit_notify(SERIAL_VIRT_RX_CH);

    /* Now that we have notified the driver, we can attempt to dequeue from the active queue.
    When the driver has processed an interrupt, it will add the inputted character to the active queue.*/

    // Address that we will pass to dequeue to store the buffer address
    uintptr_t buffer = 0;

    // Integer to store the length of the buffer
    unsigned int buffer_len = 0;

    while (serial_dequeue_active(&rx_queue, &buffer, &buffer_len) != 0) {
        /* The queue is currently empty, as there is no character to get.
        We will spin here until we have gotten a character. As the driver is a higher priority than us,
        it should be able to pre-empt this loop
        */
        microkit_dbg_puts(""); /* From Patrick, this is apparently needed to stop the compiler from optimising out the
        as it is currently empty. When compiled in a release version the puts statement will be compiled
        into a nop command.
        */
    }

    // We are only getting one character at a time, so we just need to cast the buffer to an int
    char got_char = *((char *) buffer);

    /* Now that we are finished with the active buffer, we can add it back to the free queue*/
    int ret = serial_enqueue_free(&rx_queue, buffer, buffer_len);

    if (ret != 0) {
        microkit_dbg_puts(microkit_name);
        microkit_dbg_puts(": getchar - unable to enqueue active buffer back into available queue\n");
    }

    return (int) got_char;
}

/* Return 0 on success, -1 on failure.
Basic scanf implementation using the getchar function above. Gets characters until
CTRL+C or CTRL+D or new line.
NOT MEMORY SAFE
*/
int serial_server_scanf(char *buffer)
{
    int i = 0;
    int getchar_ret = getchar();

    if (getchar_ret == -1) {
        microkit_dbg_puts("Error getting char\n");
        return -1;
    }


    while (getchar_ret != '\03' && getchar_ret != '\04' && getchar_ret != '\r') {
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

// Init function required by sel4cp, initialise serial datastructres for server here
void init(void)
{
    microkit_dbg_puts(microkit_name);
    microkit_dbg_puts(": elf PD init function running\n");

    // Here we need to init queue buffers and other data structures

    // Init the shared queue buffers
    serial_queue_init(&rx_queue, (serial_queue_t *)rx_free, (serial_queue_t *)rx_active, 0, 512, 512);
    // We will also need to populate these queues with memory from the shared dma region

    // Add buffers to the rx queue
    for (int i = 0; i < NUM_ENTRIES - 1; i++) {
        int ret = serial_enqueue_free(&rx_queue, rx_data + (i * BUFFER_SIZE), BUFFER_SIZE);

        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts(": server rx buffer population, unable to enqueue buffer\n");
        }
    }

    serial_queue_init(&tx_queue, (serial_queue_t *)tx_free, (serial_queue_t *)tx_active, 0, 512, 512);

    // Add buffers to the tx queue
    for (int i = 0; i < NUM_ENTRIES - 1; i++) {
        // Have to start at the memory region left of by the rx queue
        int ret = serial_enqueue_free(&tx_queue, tx_data + ((i + NUM_ENTRIES) * BUFFER_SIZE), BUFFER_SIZE);

        if (ret != 0) {
            microkit_dbg_puts(microkit_name);
            microkit_dbg_puts(": server tx buffer population, unable to enqueue buffer\n");
        }
    }

    // Plug the tx active queue
    serial_queue_plug(tx_queue.active);

    /* Some basic tests for the serial driver */

    serial_server_printf("Attempting to use the server printf! -- from ");
    serial_server_printf(microkit_name);
    serial_server_printf("\n");

    // Unplug the tx active queue
    serial_queue_unplug(tx_queue.active);
    microkit_notify(SERIAL_VIRT_TX_CH);

    serial_server_printf("Enter char to test getchar for ");
    serial_server_printf(microkit_name);
    serial_server_printf("\n");
    char test = getchar();
    serial_server_printf("We got the following char in ");
    serial_server_printf(microkit_name);
    serial_server_printf(" : '");
    serial_server_printf(&test);
    serial_server_printf("'\n");
    serial_server_printf("Enter char to test getchar for \n");
    serial_server_printf(microkit_name);
    serial_server_printf(": ");
    test = getchar();
    serial_server_printf("We got the following char in ");
    serial_server_printf(microkit_name);
    serial_server_printf(": ");
    serial_server_printf(&test);

    serial_server_printf("\nEnter char to test scanf in ");
    serial_server_printf(microkit_name);
    serial_server_printf("\n");

    char temp_buffer = 0;

    int scanf_ret = serial_server_scanf(&temp_buffer);

    if (scanf_ret == -1) {
        serial_server_printf("Issue with scanf\n");
    } else {
        serial_server_printf(&temp_buffer);
    }

    serial_server_printf("\n---END OF ");
    serial_server_printf(microkit_name);
    serial_server_printf(" TEST---\n");
}


void notified(microkit_channel ch)
{
    return;
}
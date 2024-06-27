#include "sddf/util/util.h"
#include <microkit.h>
#include <sddf/util/printf.h>
#include <stdbool.h>
#include <stdint.h>
#include <serial_config.h>
#include <uart.h>

serial_queue_t *rx_queue;
serial_queue_t *tx_queue;

char *rx_data;
char *tx_data;

serial_queue_handle_t* rx_queue_handle;
serial_queue_handle_t* tx_queue_handle;

uintptr_t uart_base;
volatile imx_uart_regs_t *uart_regs;

static char cml_memory[1024*2];

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
    unsigned long sz = 1024;
    unsigned long cml_heap_sz = sz;
    unsigned long cml_stack_sz = sz;
    cml_heap = cml_memory;
    cml_stack = cml_heap + cml_heap_sz;
    cml_stackend = cml_stack + cml_stack_sz;
}

void init_pancake_data() {
    uintptr_t *heap = (uintptr_t *) cml_heap;
    heap[5] = (uintptr_t) rx_data;
    heap[6] = (uintptr_t) tx_data;
    heap[7] = (uintptr_t) rx_queue;
    heap[8] = (uintptr_t) tx_queue;
    rx_queue_handle = (serial_queue_handle_t *) &heap[9];
    tx_queue_handle = (serial_queue_handle_t *) &heap[12];
    heap[15] = (volatile uintptr_t) uart_regs;
}

/*
 * BaudRate = RefFreq / (16 * (BMR + 1)/(BIR + 1) )
 * RefFreq = module clock after divider is applied.
 * Set BIR to 15 for integer division.
 * BMR and BIR are 16 bit.
 */
static void set_baud(long bps)
{
    uint32_t bmr, bir, fcr;
    fcr = uart_regs->fcr;
    fcr &= ~UART_FCR_REF_FRQ_DIV_MSK;
    fcr |= UART_FCR_REF_CLK_DIV_2;
    bir = 0xf;
    bmr = (UART_MOD_CLK / 2) / bps - 1;
    uart_regs->fcr = fcr;
    uart_regs->bir = bir;
    uart_regs->bmr = bmr;
}

static void uart_setup(void)
{
    uart_regs = (imx_uart_regs_t *) uart_base;

    /* Enable the UART */
    uart_regs->cr1 |= UART_CR1_UART_EN;

    /* Enable transmit and receive */
    uart_regs->cr2 |= UART_CR2_TX_EN;
#if !SERIAL_TX_ONLY
    uart_regs->cr2 |= UART_CR2_RX_EN;
#endif

    /* Configure stop bit length to 1 */
    uart_regs->cr2 &= ~(UART_CR2_STOP_BITS);

    /* Set data length to 8 */
    uart_regs->cr2 |= UART_CR2_WORD_SZE;

    /* Configure the reference clock and baud rate. Difficult to use automatic detection here as it requires the next incoming character to be 'a' or 'A'. */
    set_baud(UART_DEFAULT_BAUD);

    /* Disable escape sequence, parity checking and aging rx data interrupts. */
    uart_regs->cr2 &= ~UART_CR2_PARITY_EN;
    uart_regs->cr2 &= ~(UART_CR2_ESCAPE_EN | UART_CR2_ESCAPE_INT);
    uart_regs->cr2 &= ~UART_CR2_AGE_EN;

    uint32_t fcr = uart_regs->fcr;
    /* Enable receive interrupts every byte */
#if !SERIAL_TX_ONLY
    fcr &= ~UART_FCR_RXTL_MASK;
    fcr |= (1 << UART_FCR_RXTL_SHFT);
#endif

    /* Enable transmit interrupts if the write fifo drops below one byte - used when the write fifo becomes full */
    fcr &= ~UART_FCR_TXTL_MASK;
    fcr |= (2 << UART_FCR_TXTL_SHFT);

    uart_regs->fcr = fcr;
#if !SERIAL_TX_ONLY
    uart_regs->cr1 |= UART_CR1_RX_READY_INT;
#endif
}

void init(void)
{
    uart_setup();
    init_pancake_mem();
    init_pancake_data();
    serial_queue_init(rx_queue_handle, rx_queue, SERIAL_RX_DATA_REGION_SIZE_DRIV, rx_data);
    serial_queue_init(tx_queue_handle, tx_queue, SERIAL_TX_DATA_REGION_SIZE_DRIV, tx_data);
    cml_main();
}

void notified(microkit_channel ch)
{
    pnk_notified(ch);
}